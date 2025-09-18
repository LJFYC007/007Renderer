#include <algorithm>
#include <unordered_set>
#include <functional>

#include "RenderGraph.h"
#include "Utils/Logger.h"

RenderGraph::RenderGraph(ref<Device> pDevice) 
    : mpDevice(pDevice), mIsBuilt(false)
{
    LOG_DEBUG("RenderGraph created");
}

ref<RenderGraph> RenderGraph::create(ref<Device> pDevice, 
                                     const std::vector<RenderGraphNode>& nodes,
                                     const std::vector<RenderGraphConnection>& connections)
{
    auto graph = ref<RenderGraph>(new RenderGraph(pDevice));
    
    if (graph->build(nodes, connections))
    {
        LOG_INFO("Render graph created successfully with {} nodes and {} connections", 
                nodes.size(), connections.size());
        return graph;
    }
    else
    {
        LOG_ERROR("Failed to create render graph");
        return nullptr;
    }
}

bool RenderGraph::build(const std::vector<RenderGraphNode>& nodes, 
                       const std::vector<RenderGraphConnection>& connections)
{
    mNodes = nodes;
    mConnections = connections;
    mIsBuilt = false;

    LOG_DEBUG("Building render graph with {} passes and {} connections", mNodes.size(), mConnections.size());

    if (!validateGraph())
        return false;

    // Build dependency graph
    for (const auto& conn : mConnections)
    {
        int fromNodeIndex = findNode(conn.fromPass);
        int toNodeIndex = findNode(conn.toPass);
        
        // We already validated these exist in validateGraph()
        mNodes[toNodeIndex].dependencies.insert(fromNodeIndex);
    }

    if (!topologicalSort())
    {
        LOG_ERROR("Topological sort failed - circular dependency detected");
        return false;
    }

    mIsBuilt = true;
    LOG_INFO("Render graph built successfully");
    return true;
}

bool RenderGraph::validateGraph()
{
    // Check for duplicate node names
    std::unordered_set<std::string> nodeNames;
    for (const auto& node : mNodes)
    {
        if (nodeNames.count(node.name))
        {
            LOG_ERROR("Duplicate node name found: '{}'", node.name);
            return false;
        }
        nodeNames.insert(node.name);
    }

    // Check for self-loops and multiple inputs to same port
    std::unordered_set<std::string> inputKeys;
    for (const auto& conn : mConnections)
    {
        // Check for self-loop
        if (conn.fromPass == conn.toPass)
        {
            LOG_ERROR("Self-loop detected: Pass '{}' connects to itself", conn.fromPass);
            return false;
        }

        // Check for multiple connections to the same input
        std::string inputKey = conn.toPass + "." + conn.toInput;
        if (inputKeys.count(inputKey))
        {
            LOG_ERROR("Multiple connections to input '{}' in pass '{}'", conn.toInput, conn.toPass);
            return false;
        }
        inputKeys.insert(inputKey);
    }

    // Validate node and port existence
    for (const auto& conn : mConnections)
    {
        int fromNodeIndex = findNode(conn.fromPass);
        int toNodeIndex = findNode(conn.toPass);
        if (fromNodeIndex == -1 || toNodeIndex == -1)
        {
            LOG_ERROR("Connection validation failed: Pass '{}' -> '{}' not found", conn.fromPass, conn.toPass);
            return false;
        }

        // Check if output exists
        bool outputFound = false;
        for (const auto& output : mNodes[fromNodeIndex].pass->getOutputs())
        {
            if (output.name == conn.fromOutput)
            {
                outputFound = true;
                break;
            }
        }
        if (!outputFound)
        {
            LOG_ERROR("Output '{}' not found in pass '{}'", conn.fromOutput, conn.fromPass);
            return false;
        }

        // Check if input exists
        bool inputFound = false;
        for (const auto& input : mNodes[toNodeIndex].pass->getInputs())
        {
            if (input.name == conn.toInput)
            {
                inputFound = true;
                break;
            }
        }
        if (!inputFound)
        {
            LOG_ERROR("Input '{}' not found in pass '{}'", conn.toInput, conn.toPass);
            return false;
        }
    }

    return true;
}

bool RenderGraph::topologicalSort()
{
    mExecutionOrder.clear();
    std::unordered_set<uint> visited;
    std::unordered_set<uint> inStack;

    // DFS-based topological sort with cycle detection
    std::function<bool(uint)> dfs = [&](uint nodeIndex) -> bool
    {
        if (inStack.count(nodeIndex))
            return false; // Cycle detected
        if (visited.count(nodeIndex))
            return true; // Already processed

        inStack.insert(nodeIndex);
        for (const auto& depIndex : mNodes[nodeIndex].dependencies)
        {
            if (!dfs(depIndex))
                return false;
        }
        inStack.erase(nodeIndex);
        visited.insert(nodeIndex);
        mExecutionOrder.push_back(nodeIndex);
        return true;
    };

    // Visit all nodes
    for (uint i = 0; i < mNodes.size(); ++i)
    {
        if (!dfs(i))
            return false;
    }
    return true;
}

RenderData RenderGraph::execute()
{
    if (!mIsBuilt)
    {
        LOG_ERROR("Cannot execute render graph - not built");
        return RenderData();
    }

    mIntermediateResults.clear();
    RenderData finalOutput;

    for (const auto& nodeIndex : mExecutionOrder)
    {
        RenderData result = executePass(nodeIndex);
        mIntermediateResults[mNodes[nodeIndex].name] = result;
        
        // Add all outputs to final result with namespaced keys
        for (const auto& output : mNodes[nodeIndex].pass->getOutputs())
        {
            finalOutput[mNodes[nodeIndex].name + "." + output.name] = result[output.name];
        }
    }
    return finalOutput;
}

RenderData RenderGraph::executePass(int nodeIndex)
{
    // Build input for this pass by collecting from connected passes
    RenderData input;
    for (const auto& conn : mConnections)
    {
        if (conn.toPass == mNodes[nodeIndex].name)
        {
            auto it = mIntermediateResults.find(conn.fromPass);
            if (it != mIntermediateResults.end())
            {
                auto resource = it->second[conn.fromOutput];
                if (resource)
                    input[conn.toInput] = resource;
            }
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
    auto it = std::find_if(mNodes.begin(), mNodes.end(), 
        [&name](const RenderGraphNode& node) { return node.name == name; });
    return it != mNodes.end() ? static_cast<int>(it - mNodes.begin()) : -1;
}
