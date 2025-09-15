#include <algorithm>
#include <unordered_set>

#include "RenderGraph.h"
#include "Utils/Logger.h"

void RenderGraph::addPass(const std::string& name, ref<RenderPass> pass)
{
    mNodes.emplace_back(name, std::move(pass));
}

void RenderGraph::addConnection(const std::string& fromPass, const std::string& fromOutput, const std::string& toPass, const std::string& toInput)
{
    mConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
}

bool RenderGraph::build()
{
    LOG_DEBUG("Building render graph with {} passes and {} connections", mNodes.size(), mConnections.size());

    // Build dependency graph
    for (const auto& conn : mConnections)
    {
        int fromNodeIndex = findNode(conn.fromPass);
        int toNodeIndex = findNode(conn.toPass);
        if (fromNodeIndex == -1 || toNodeIndex == -1)
        {
            LOG_ERROR("Connection build failed: Pass '{}' -> '{}' not found", conn.fromPass, conn.toPass);
            return false;
        }

        // Check if output exists
        bool outputFound = false;
        for (const auto& output : mNodes[fromNodeIndex].pass->getOutputs())
            if (output.name == conn.fromOutput)
            {
                outputFound = true;
                break;
            }
        if (!outputFound)
        {
            LOG_ERROR("Connection validation failed: Output '{}' not found in pass '{}'", conn.fromOutput, conn.fromPass);
            return false;
        }

        // Check if input exists
        bool inputFound = false;
        for (const auto& input : mNodes[toNodeIndex].pass->getInputs())
            if (input.name == conn.toInput)
            {
                inputFound = true;
                break;
            }
        if (!inputFound)
        {
            LOG_ERROR("Connection validation failed: Input '{}' not found in pass '{}'", conn.toInput, conn.toPass);
            return false;
        }

        mNodes[toNodeIndex].dependencies.insert(conn.fromPass);
    }

    if (!topologicalSort())
    {
        LOG_ERROR("Topological sort failed - circular dependency detected");
        return false;
    }

    LOG_INFO("Render graph built successfully.");
    return true;
}

bool RenderGraph::topologicalSort()
{
    mExecutionOrder.clear();
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inStack;

    // DFS-based topological sort with cycle detection
    std::function<bool(const std::string&)> dfs = [&](const std::string& nodeName) -> bool
    {
        if (inStack.count(nodeName))
            return false; // Cycle detected
        if (visited.count(nodeName))
            return true; // Already processed

        inStack.insert(nodeName);
        int nodeIndex = findNode(nodeName);
        for (const auto& dep : mNodes[nodeIndex].dependencies)
            if (!dfs(dep))
                return false;
        inStack.erase(nodeName);
        visited.insert(nodeName);
        mExecutionOrder.push_back(nodeName);
        return true;
    };

    // Visit all nodes
    for (const auto& node : mNodes)
        if (!dfs(node.name))
            return false;
    return true;
}

RenderData RenderGraph::execute()
{
    std::unordered_map<std::string, RenderData> intermediateResults;
    RenderData finalOutput;

    for (const auto& passName : mExecutionOrder)
    {
        int nodeIndex = findNode(passName);
        RenderData result = executePass(nodeIndex, intermediateResults);
        intermediateResults[passName] = result;
        for (const auto& output : mNodes[nodeIndex].pass->getOutputs())
            finalOutput[passName + "." + output.name] = result[output.name];
    }
    return finalOutput;
}

RenderData RenderGraph::executePass(int nodeIndex, const std::unordered_map<std::string, RenderData>& intermediateResults)
{
    // Build input for this pass by collecting from connected passes
    RenderData input;
    for (const auto& conn : mConnections)
    {
        if (conn.toPass == mNodes[nodeIndex].name)
        {
            auto it = intermediateResults.find(conn.fromPass);
            if (it != intermediateResults.end())
            {
                auto resource = it->second[conn.fromOutput];
                if (resource)
                    input[conn.toInput] = resource;
            }
        }
    }

    return mNodes[nodeIndex].pass->execute(input);
}

void RenderGraph::setScene(ref<Scene> scene)
{
    mpScene = scene;
    for (auto& node : mNodes)
        node.pass->setScene(scene);
}

void RenderGraph::renderUI()
{
    for (auto& node : mNodes)
    {
        if (GUI::CollapsingHeader(node.name.c_str()))
            node.pass->renderUI();
    }
}

int RenderGraph::findNode(const std::string& name)
{
    auto it = std::find_if(mNodes.begin(), mNodes.end(), [&name](const RenderGraphNode& node) { return node.name == name; });
    return it != mNodes.end() ? static_cast<int>(&(*it) - &mNodes[0]) : -1;
}
