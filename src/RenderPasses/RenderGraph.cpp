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

void RenderGraph::build()
{
    LOG_DEBUG("Building render graph with {} passes and {} connections", mNodes.size(), mConnections.size());

    // Check for duplicate node names
    std::unordered_set<std::string> nodeNames;
    for (const auto& node : mNodes)
    {
        if (nodeNames.count(node.name))
            LOG_ERROR_RETURN("Duplicate node name found: '{}'", node.name);
        nodeNames.insert(node.name);
    }

    // Check for self-loops and multiple inputs to same port
    std::unordered_set<std::string> inputKeys;
    for (const auto& conn : mConnections)
    {
        // Check for self-loop
        if (conn.fromPass == conn.toPass)
            LOG_ERROR_RETURN("Self-loop detected: Pass '{}' connects to itself", conn.fromPass);

        // Check for multiple connections to the same input
        std::string inputKey = conn.toPass + "." + conn.toInput;
        if (inputKeys.count(inputKey))
            LOG_ERROR_RETURN("Multiple connections to input '{}' in pass '{}'", conn.toInput, conn.toPass);
        inputKeys.insert(inputKey);
    }

    // Build dependency graph
    for (const auto& conn : mConnections)
    {
        int fromNodeIndex = findNode(conn.fromPass);
        int toNodeIndex = findNode(conn.toPass);
        if (fromNodeIndex == -1 || toNodeIndex == -1)
            LOG_ERROR_RETURN("Connection build failed: Pass '{}' -> '{}' not found", conn.fromPass, conn.toPass);

        // Check if output exists
        bool outputFound = false;
        for (const auto& output : mNodes[fromNodeIndex].pass->getOutputs())
            if (output.name == conn.fromOutput)
            {
                outputFound = true;
                break;
            }
        if (!outputFound)
            LOG_ERROR_RETURN("Connection validation failed: Output '{}' not found in pass '{}'", conn.fromOutput, conn.fromPass);

        // Check if input exists
        bool inputFound = false;
        for (const auto& input : mNodes[toNodeIndex].pass->getInputs())
            if (input.name == conn.toInput)
            {
                inputFound = true;
                break;
            }
        if (!inputFound)
            LOG_ERROR_RETURN("Connection validation failed: Input '{}' not found in pass '{}'", conn.toInput, conn.toPass);

        mNodes[toNodeIndex].dependencies.insert(fromNodeIndex);
    }

    if (!topologicalSort())
        LOG_ERROR_RETURN("Topological sort failed - circular dependency detected");

    LOG_INFO("Render graph built successfully.");
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
            if (!dfs(depIndex))
                return false;
        inStack.erase(nodeIndex);
        visited.insert(nodeIndex);
        mExecutionOrder.push_back(nodeIndex);
        return true;
    };

    // Visit all nodes
    for (uint i = 0; i < mNodes.size(); ++i)
        if (!dfs(i))
            return false;
    return true;
}

RenderData RenderGraph::execute()
{
    intermediateResults.clear();
    RenderData finalOutput;

    for (const auto& nodeIndex : mExecutionOrder)
    {
        RenderData result = executePass(nodeIndex);
        intermediateResults[mNodes[nodeIndex].name] = result;
        for (const auto& output : mNodes[nodeIndex].pass->getOutputs())
            finalOutput[mNodes[nodeIndex].name + "." + output.name] = result[output.name];
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
    // Render graph visualization
    if (GUI::CollapsingHeader("Render Graph Visualization"))
    {
        renderGraphVisualization();
    }

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

void RenderGraph::renderGraphVisualization()
{
    // Helper function to convert RenderDataType to string
    auto renderDataTypeToString = [](RenderDataType type) -> const char*
    {
        switch (type)
        {
        case RenderDataType::Texture2D:
            return "Texture2D";
        case RenderDataType::Buffer:
            return "Buffer";
        case RenderDataType::Unknown:
            return "Unknown";
        default:
            return "Unknown";
        }
    };

    GUI::Separator();

    // Display graph statistics
    GUI::Text("Graph Statistics:");
    GUI::BulletText("Nodes: %d", static_cast<int>(mNodes.size()));
    GUI::BulletText("Connections: %d", static_cast<int>(mConnections.size()));
    GUI::BulletText("Built: %s", mExecutionOrder.empty() ? "No" : "Yes");
    GUI::Separator();

    // Create a table for nodes and connections display
    if (GUI::BeginTable("GraphVisualization", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV))
    {
        GUI::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 400.0f);
        GUI::TableSetupColumn("Connections", ImGuiTableColumnFlags_WidthStretch);
        GUI::TableHeadersRow();

        GUI::TableNextRow();

        // Left column: Nodes
        GUI::TableNextColumn();
        GUI::Text("Render Passes:");
        GUI::Separator();

        for (size_t i = 0; i < mNodes.size(); ++i)
        {
            const auto& node = mNodes[i];

            // Node header with execution order info
            GUI::PushID(static_cast<int>(i));
            bool nodeExpanded = GUI::TreeNodeEx(node.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            // Add execution order indicator
            GUI::SameLine();
            if (!mExecutionOrder.empty())
            {
                auto execIt = std::find(mExecutionOrder.begin(), mExecutionOrder.end(), i);
                if (execIt != mExecutionOrder.end())
                {
                    int execOrder = static_cast<int>(execIt - mExecutionOrder.begin());
                    GUI::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "[Order: %d]", execOrder);
                }
            }

            if (nodeExpanded)
            {
                GUI::Indent();

                // Display inputs
                const auto& inputs = node.pass->getInputs();
                if (!inputs.empty())
                {
                    GUI::Text("Inputs:");
                    for (const auto& input : inputs)
                        GUI::BulletText("%s (%s)", input.name.c_str(), renderDataTypeToString(input.type));
                }

                // Display outputs
                const auto& outputs = node.pass->getOutputs();
                if (!outputs.empty())
                {
                    GUI::Text("Outputs:");
                    for (const auto& output : outputs)
                        GUI::BulletText("%s (%s)", output.name.c_str(), renderDataTypeToString(output.type));
                }

                // Display dependencies
                if (!node.dependencies.empty())
                {
                    GUI::Text("Dependencies:");
                    for (const auto& depIndex : node.dependencies)
                        if (depIndex < mNodes.size())
                            GUI::BulletText("%s", mNodes[depIndex].name.c_str());
                }

                GUI::Unindent();
                GUI::TreePop();
            }
            GUI::PopID();
        }

        // Right column: Connections
        GUI::TableNextColumn();
        GUI::Text("Data Flow:");
        GUI::Separator();

        if (mConnections.empty())
        {
            GUI::Text("No connections");
        }
        else
        {
            // Group connections by from pass for better readability
            std::unordered_map<std::string, std::vector<const RenderGraphConnection*>> connectionsBySource;
            for (const auto& conn : mConnections)
                connectionsBySource[conn.fromPass].push_back(&conn);

            for (const auto& [fromPass, connections] : connectionsBySource)
            {
                GUI::PushID(fromPass.c_str());
                if (GUI::TreeNodeEx(fromPass.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    GUI::Indent();
                    for (const auto* conn : connections)
                    {
                        // Color-coded connection display
                        GUI::TextColored(ImVec4(0.8f, 0.8f, 0.6f, 1.0f), "%s", conn->fromOutput.c_str());
                        GUI::SameLine();
                        GUI::Text(" -> ");
                        GUI::SameLine();
                        GUI::TextColored(ImVec4(0.6f, 0.8f, 0.8f, 1.0f), "%s", conn->toPass.c_str());
                        GUI::SameLine();
                        GUI::Text(".");
                        GUI::SameLine();
                        GUI::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "%s", conn->toInput.c_str());
                    }
                    GUI::Unindent();
                    GUI::TreePop();
                }
                GUI::PopID();
            }
        }

        GUI::EndTable();
    }

    GUI::Separator();

    // Execution order visualization
    if (!mExecutionOrder.empty())
    {
        GUI::Text("Execution Order:");
        for (size_t i = 0; i < mExecutionOrder.size(); ++i)
        {
            if (i > 0)
            {
                GUI::SameLine();
                GUI::Text(" -> ");
                GUI::SameLine();
            }

            const auto& nodeName = mNodes[mExecutionOrder[i]].name;
            GUI::TextColored(ImVec4(0.9f, 0.7f, 0.5f, 1.0f), "%s", nodeName.c_str());
        }
    }
    else
    {
        GUI::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Graph not built - call build() first");
    }
}
