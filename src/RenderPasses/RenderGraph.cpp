#include <algorithm>
#include <unordered_set>
#include <functional>

#include "RenderGraph.h"
#include "Utils/Logger.h"

ref<RenderGraph> RenderGraph::create(
    ref<Device> pDevice,
    const std::vector<RenderGraphNode>& nodes,
    const std::vector<RenderGraphConnection>& connections
)
{
    auto graph = ref<RenderGraph>(new RenderGraph(pDevice));
    if (!graph->build(nodes, connections))
        return nullptr;
    return graph;
}

bool RenderGraph::build(const std::vector<RenderGraphNode>& nodes, const std::vector<RenderGraphConnection>& connections)
{
    mNodes = nodes;
    mConnections = connections;

    // Initialize data structures
    mDependencies.resize(mNodes.size());
    mExecutionOrder.clear();
    mAvailableOutputs.clear();
    for (uint i = 0; i < mNodes.size(); ++i)
        mDependencies[i].clear();

    LOG_DEBUG("Building render graph with {} passes and {} connections", mNodes.size(), mConnections.size());
    if (!validateGraph())
        return false;
    buildDependencyGraph();

    if (!topologicalSort())
    {
        LOG_ERROR("Topological sort failed - circular dependency detected");
        return false;
    }
    LOG_INFO("Render graph built successfully");
    return true;
}

void RenderGraph::buildDependencyGraph()
{
    for (const auto& conn : mConnections)
    {
        int fromNodeIndex = findNode(conn.fromPass);
        int toNodeIndex = findNode(conn.toPass);
        mDependencies[toNodeIndex].insert(fromNodeIndex);
    }
}

bool RenderGraph::validateGraph()
{
    std::unordered_set<std::string> nodeNames;
    std::unordered_set<std::string> inputKeys;

    // Validate node names are unique
    for (const auto& node : mNodes)
    {
        if (nodeNames.count(node.name))
        {
            LOG_ERROR("Duplicate node name found: '{}'", node.name);
            return false;
        }
        nodeNames.insert(node.name);
    }

    // Validate connections
    for (const auto& conn : mConnections)
    {
        if (conn.fromPass == conn.toPass)
        {
            LOG_ERROR("Self-loop detected: Pass '{}' connects to itself", conn.fromPass);
            return false;
        }

        std::string inputKey = conn.toPass + "." + conn.toInput;
        if (inputKeys.count(inputKey))
        {
            LOG_ERROR("Multiple connections to input '{}' in pass '{}'", conn.toInput, conn.toPass);
            return false;
        }
        inputKeys.insert(inputKey);
    }

    // Validate nodes and ports exist
    for (const auto& conn : mConnections)
    {
        int fromNodeIndex = findNode(conn.fromPass);
        int toNodeIndex = findNode(conn.toPass);
        if (fromNodeIndex == -1 || toNodeIndex == -1)
        {
            LOG_ERROR("Connection validation failed: Pass '{}' -> '{}' not found", conn.fromPass, conn.toPass);
            return false;
        }

        const auto& outputs = mNodes[fromNodeIndex].pass->getOutputs();
        bool outputFound =
            std::any_of(outputs.begin(), outputs.end(), [&conn](const RenderPassOutput& output) { return output.name == conn.fromOutput; });
        if (!outputFound)
        {
            LOG_ERROR("Output '{}' not found in pass '{}'", conn.fromOutput, conn.fromPass);
            return false;
        }

        const auto& inputs = mNodes[toNodeIndex].pass->getInputs();
        bool inputFound = std::any_of(inputs.begin(), inputs.end(), [&conn](const RenderPassInput& input) { return input.name == conn.toInput; });
        if (!inputFound)
        {
            LOG_ERROR("Input '{}' not found in pass '{}'", conn.toInput, conn.toPass);
            return false;
        }
    }

    // Validate all required inputs are connected
    for (const auto& node : mNodes)
        for (const auto& input : node.pass->getInputs())
            if (!input.optional)
            {
                std::string inputKey = node.name + "." + input.name;
                if (!inputKeys.count(inputKey))
                {
                    LOG_ERROR("Required input '{}' in pass '{}' is not connected", input.name, node.name);
                    return false;
                }
            }

    // Collect available outputs for UI selection
    for (const auto& node : mNodes)
        for (const auto& output : node.pass->getOutputs())
        {
            std::string outputKey = node.name + "." + output.name;
            mAvailableOutputs.push_back(outputKey);
        }

    // Set the last one is default output selection
    if (mSelectedOutputKey.empty() && !mAvailableOutputs.empty())
    {
        mSelectedOutputIndex = static_cast<uint>(mAvailableOutputs.size() - 1);
        mSelectedOutputKey = mAvailableOutputs[mSelectedOutputIndex];
    }
    return true;
}

bool RenderGraph::topologicalSort()
{
    mExecutionOrder.clear();
    std::unordered_set<uint> visited;
    std::unordered_set<uint> inStack;

    std::function<bool(uint)> dfs = [&](uint nodeIndex) -> bool
    {
        if (inStack.count(nodeIndex))
            return false; // Cycle detected
        if (visited.count(nodeIndex))
            return true;

        inStack.insert(nodeIndex);
        for (const auto& depIndex : mDependencies[nodeIndex])
        {
            if (!dfs(depIndex))
                return false;
        }
        inStack.erase(nodeIndex);
        visited.insert(nodeIndex);
        mExecutionOrder.push_back(nodeIndex);
        return true;
    };

    for (uint i = 0; i < mNodes.size(); ++i)
        if (!dfs(i))
            return false;
    return true;
}

RenderData RenderGraph::execute()
{
    mIntermediateResults.clear();
    RenderData finalOutput;
    for (const auto& nodeIndex : mExecutionOrder)
    {
        RenderData result = executePass(nodeIndex);
        mIntermediateResults[mNodes[nodeIndex].name] = result;
        for (const auto& output : mNodes[nodeIndex].pass->getOutputs())
            finalOutput[mNodes[nodeIndex].name + "." + output.name] = result[output.name];
    }
    return finalOutput;
}

RenderData RenderGraph::executePass(int nodeIndex)
{
    RenderData input;
    for (const auto& conn : mConnections)
    {
        if (conn.toPass == mNodes[nodeIndex].name)
        {
            auto it = mIntermediateResults.find(conn.fromPass);
            auto resource = it->second[conn.fromOutput];
            input[conn.toInput] = resource;
        }
    }
    return mNodes[nodeIndex].pass->execute(input);
}

void RenderGraph::setScene(ref<Scene> pScene)
{
    mpScene = pScene;
    for (auto& node : mNodes)
        node.pass->setScene(pScene);
}

int RenderGraph::findNode(const std::string& name) const
{
    auto it = std::find_if(mNodes.begin(), mNodes.end(), [&name](const RenderGraphNode& node) { return node.name == name; });
    return it != mNodes.end() ? static_cast<int>(it - mNodes.begin()) : -1;
}

nvrhi::TextureHandle RenderGraph::getFinalOutputTexture()
{
    size_t dotPos = mSelectedOutputKey.find('.');
    std::string passName = mSelectedOutputKey.substr(0, dotPos);
    std::string outputName = mSelectedOutputKey.substr(dotPos + 1);
    auto it = mIntermediateResults.find(passName);
    nvrhi::TextureHandle sourceTexture = static_cast<nvrhi::ITexture*>(it->second[outputName].Get());
    const auto& sourceDesc = sourceTexture->getDesc();

    // Check if we need to recreate the output texture
    if (!mOutputTexture || mOutputWidth != sourceDesc.width || mOutputHeight != sourceDesc.height)
        createOutputTexture(sourceDesc.width, sourceDesc.height, sourceDesc.format);

    // Copy the source texture to our managed output texture
    auto commandList = mpDevice->getCommandList();
    commandList->open();
    nvrhi::TextureSlice slice;
    commandList->copyTexture(mOutputTexture, slice, sourceTexture, slice);
    commandList->close();
    mpDevice->getDevice()->executeCommandList(commandList);

    return mOutputTexture;
}

void RenderGraph::createOutputTexture(uint32_t width, uint32_t height, nvrhi::Format format)
{
    nvrhi::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.dimension = nvrhi::TextureDimension::Texture2D;
    desc.mipLevels = 1;
    desc.arraySize = 1;
    desc.sampleCount = 1;
    desc.isRenderTarget = false;
    desc.isUAV = false;
    desc.initialState = nvrhi::ResourceStates::ShaderResource;
    desc.keepInitialState = true;
    desc.debugName = "RenderGraph/OutputTexture";

    mOutputTexture = mpDevice->getDevice()->createTexture(desc);
    mOutputWidth = width;
    mOutputHeight = height;
}

void RenderGraph::renderOutputSelectionUI()
{
    std::vector<const char*> outputNames;
    outputNames.reserve(mAvailableOutputs.size());
    for (const auto& output : mAvailableOutputs)
        outputNames.push_back(output.c_str());

    if (GUI::Combo("Output", &mSelectedOutputIndex, outputNames.data(), static_cast<int>(outputNames.size())))
        mSelectedOutputKey = mAvailableOutputs[mSelectedOutputIndex];
}
