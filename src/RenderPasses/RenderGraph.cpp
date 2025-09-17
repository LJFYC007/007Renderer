#include <algorithm>
#include <unordered_set>
#include <string>

#include "RenderGraph.h"
#include "Utils/Logger.h"

RenderGraph::RenderGraph(ref<Device> device) : mpDevice(device), mpEditorContext(nullptr), mNextId(1)
{
    ed::Config config;
    config.SettingsFile = (std::string(PROJECT_LOG_DIR) + "/editor.json").c_str();
    mpEditorContext = ed::CreateEditor(&config);
    if (!mpEditorContext)
        LOG_ERROR_RETURN("Failed to create node editor context");
    
    LOG_INFO("Node editor initialized successfully");
}

RenderGraph::~RenderGraph()
{
    ed::DestroyEditor(mpEditorContext);
}

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
    for (auto& node : mNodes)
        if (GUI::CollapsingHeader(node.name.c_str()))
            node.pass->renderUI();
}

int RenderGraph::findNode(const std::string& name)
{
    auto it = std::find_if(mNodes.begin(), mNodes.end(), [&name](const RenderGraphNode& node) { return node.name == name; });
    return it != mNodes.end() ? static_cast<int>(&(*it) - &mNodes[0]) : -1;
}

void RenderGraph::renderNodeEditor()
{
    if (!mpEditorContext)
    {
        GUI::Text("Node editor not initialized!");
        return;
    }

    // Control panel
    if (GUI::Button("Auto Layout"))
        autoLayoutNodes();
    GUI::SameLine();
    if (GUI::Button("Center View"))
    {
        ed::SetCurrentEditor(mpEditorContext);
        ed::NavigateToContent(0.0f);
        ed::SetCurrentEditor(nullptr);
    }
    GUI::SameLine();
    GUI::Text("Nodes: %zu | Connections: %zu", mNodes.size(), mConnections.size());
    
    GUI::Separator();
    
    ed::SetCurrentEditor(mpEditorContext);
    
    // Node editor styling
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));
    ed::PushStyleVar(ed::StyleVar_NodeRounding, 4.0f);
    
    ed::Begin("Node Editor", ImVec2(0.0f, 0.0f));
    
    // Initialize IDs and position nodes on first frame
    initializeNodeIds();
    static bool sFirstFrame = true;
    if (sFirstFrame)
    {
        for (size_t i = 0; i < mNodes.size(); ++i)
        {
            ImVec2 pos = calculateNodePosition(mNodes[i].name, i);
            ed::SetNodePosition(mNodeIds[mNodes[i].name], pos);
        }
        sFirstFrame = false;
    }
    
    // Draw nodes and connections
    drawNodes();
    drawConnections();
    handleNodeEditorInput();
    
    ed::End();
    
    // Cleanup styling
    ed::PopStyleVar(2);
    ed::PopStyleColor(2);
    ed::SetCurrentEditor(nullptr);
}

ImVec2 RenderGraph::calculateNodePosition(const std::string& nodeName, size_t nodeIndex)
{
    const float kNodeSpacing = 250.0f;
    const float kVerticalSpacing = 150.0f;
    const float kStartX = 50.0f;
    const float kStartY = 50.0f;
    
    // Try to position nodes based on their dependencies for better layout
    int nodeIdx = findNode(nodeName);
    if (nodeIdx != -1 && !mExecutionOrder.empty())
    {
        // Find position in execution order
        auto execIt = std::find(mExecutionOrder.begin(), mExecutionOrder.end(), nodeIdx);
        if (execIt != mExecutionOrder.end())
        {
            size_t execIndex = execIt - mExecutionOrder.begin();
            float x = kStartX + (execIndex % 4) * kNodeSpacing;
            float y = kStartY + (execIndex / 4) * kVerticalSpacing;
            return ImVec2(x, y);
        }
    }
    
    // Fallback to simple grid layout
    float x = kStartX + (nodeIndex % 3) * kNodeSpacing;
    float y = kStartY + (nodeIndex / 3) * kVerticalSpacing;
    return ImVec2(x, y);
}

void RenderGraph::initializeNodeIds()
{
    for (size_t i = 0; i < mNodes.size(); ++i)
    {
        const auto& node = mNodes[i];
        if (mNodeIds.find(node.name) == mNodeIds.end())
        {
            mNodeIds[node.name] = mNextId++;
            
            // Create pin IDs for inputs
            const auto& inputs = node.pass->getInputs();
            mInputPinIds[node.name].clear();
            for (size_t j = 0; j < inputs.size(); ++j)
                mInputPinIds[node.name].push_back(mNextId++);
            
            // Create pin IDs for outputs
            const auto& outputs = node.pass->getOutputs();
            mOutputPinIds[node.name].clear();
            for (size_t j = 0; j < outputs.size(); ++j)
                mOutputPinIds[node.name].push_back(mNextId++);
        }
    }
}

// Removed positionNodesInitially - now using calculateNodePosition in renderNodeEditor

void RenderGraph::drawNodes()
{
    auto getTypeString = [](RenderDataType type) -> const char*
    {
        switch (type)
        {
        case RenderDataType::Texture2D: return "Texture2D";
        case RenderDataType::Buffer:    return "Buffer";
        case RenderDataType::Unknown:
        default:                        return "Unknown";
        }
    };

    // Tooltip state for deferred rendering (following widgets-example pattern)
    static bool sShowTooltip = false;
    static std::string sTooltipText;
    sShowTooltip = false;

    int uniqueId = 1;
    
    for (const auto& node : mNodes)
    {
        int nodeId = mNodeIds[node.name];
        
        ed::BeginNode(nodeId);
            const auto& inputs = node.pass->getInputs();
            const auto& outputs = node.pass->getOutputs();
            
            // Node title
            GUI::Text("%s", node.name.c_str());
            
            // Pin layout: Input pin - spacer - Output pin
            if (!inputs.empty())
            {
                ed::BeginPin(mInputPinIds[node.name][0], ed::PinKind::Input);
                    GUI::Text("-> In");
                ed::EndPin();
            }
            
            GUI::SameLine();
            GUI::Dummy(ImVec2(150, 0));
            GUI::SameLine();
            
            if (!outputs.empty())
            {
                ed::BeginPin(mOutputPinIds[node.name][0], ed::PinKind::Output);
                    GUI::Text("Out ->");
                ed::EndPin();
            }
            
            // Input details
            if (!inputs.empty())
            {
                GUI::Spacing();
                GUI::Text("Inputs:");
                for (size_t i = 0; i < inputs.size(); ++i)
                {
                    GUI::PushID(uniqueId++);
                    GUI::BulletText("%s (%s)", inputs[i].name.c_str(), getTypeString(inputs[i].type));
                    if (GUI::IsItemHovered())
                    {
                        sShowTooltip = true;
                        sTooltipText = "Input: " + inputs[i].name + 
                                     "\nType: " + getTypeString(inputs[i].type) +
                                     "\nOptional: " + (inputs[i].optional ? "Yes" : "No");
                    }
                    GUI::PopID();
                }
            }
            
            // Output details  
            if (!outputs.empty())
            {
                if (!inputs.empty()) GUI::Spacing();
                GUI::Text("Outputs:");
                for (size_t i = 0; i < outputs.size(); ++i)
                {
                    GUI::PushID(uniqueId++);
                    GUI::BulletText("%s (%s)", outputs[i].name.c_str(), getTypeString(outputs[i].type));
                    if (GUI::IsItemHovered())
                    {
                        sShowTooltip = true;
                        sTooltipText = "Output: " + outputs[i].name + 
                                     "\nType: " + getTypeString(outputs[i].type);
                    }
                    GUI::PopID();
                }
            }
            
            // Minimum node width
            GUI::Dummy(ImVec2(250, 1));
            
        ed::EndNode();
    }
    
    // Deferred tooltip section (must be outside node drawing)
    if (sShowTooltip)
    {
        ed::Suspend();
        GUI::SetTooltip("%s", sTooltipText.c_str());
        ed::Resume();
    }
}

void RenderGraph::drawConnections()
{
    // Initialize link IDs if needed
    if (mLinkIds.empty())
    {
        for (const auto& conn : mConnections)
        {
            std::string key = conn.fromPass + "." + conn.fromOutput + "->" + conn.toPass + "." + conn.toInput;
            mLinkIds[key] = mNextId++;
        }
    }
    
    // Draw all connections
    for (const auto& conn : mConnections)
    {
        std::string key = conn.fromPass + "." + conn.fromOutput + "->" + conn.toPass + "." + conn.toInput;
        
        if (mLinkIds.find(key) == mLinkIds.end())
            mLinkIds[key] = mNextId++;
        
        int fromPin = findOutputPinId(conn.fromPass, conn.fromOutput);
        int toPin = findInputPinId(conn.toPass, conn.toInput);
        
        if (fromPin != -1 && toPin != -1)
            ed::Link(mLinkIds[key], fromPin, toPin);
    }
}

int RenderGraph::findOutputPinId(const std::string& passName, const std::string& outputName)
{
    auto pinMapIt = mOutputPinIds.find(passName);
    if (pinMapIt == mOutputPinIds.end())
        return -1;
        
    auto nodeIt = std::find_if(mNodes.begin(), mNodes.end(),
        [&passName](const RenderGraphNode& n) { return n.name == passName; });
    if (nodeIt == mNodes.end())
        return -1;
        
    const auto& outputs = nodeIt->pass->getOutputs();
    for (size_t i = 0; i < outputs.size() && i < pinMapIt->second.size(); ++i)
    {
        if (outputs[i].name == outputName)
            return pinMapIt->second[i];
    }
    
    return -1;
}

int RenderGraph::findInputPinId(const std::string& passName, const std::string& inputName)
{
    auto pinMapIt = mInputPinIds.find(passName);
    if (pinMapIt == mInputPinIds.end())
        return -1;
        
    auto nodeIt = std::find_if(mNodes.begin(), mNodes.end(),
        [&passName](const RenderGraphNode& n) { return n.name == passName; });
    if (nodeIt == mNodes.end())
        return -1;
        
    const auto& inputs = nodeIt->pass->getInputs();
    for (size_t i = 0; i < inputs.size() && i < pinMapIt->second.size(); ++i)
    {
        if (inputs[i].name == inputName)
            return pinMapIt->second[i];
    }
    
    return -1;
}

void RenderGraph::handleNodeEditorInput()
{
    // Handle new connection creation
    if (ed::BeginCreate())
    {
        ed::PinId inputPin, outputPin;
        if (ed::QueryNewLink(&inputPin, &outputPin))
        {
            if (inputPin && outputPin)
            {
                std::string fromPass, fromOutput, toPass, toInput;
                if (findConnectionDetails(outputPin, inputPin, fromPass, fromOutput, toPass, toInput))
                {
                    if (ed::AcceptNewItem())
                    {
                        addConnection(fromPass, fromOutput, toPass, toInput);
                        mLinkIds.clear(); // Force regeneration
                        build(); // Rebuild dependencies
                    }
                }
                else
                {
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                }
            }
        }
    }
    ed::EndCreate();
    
    // Handle connection deletion
    if (ed::BeginDelete())
    {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId))
        {
            if (ed::AcceptDeletedItem())
            {
                removeConnectionByLinkId(linkId.Get());
            }
        }
    }
    ed::EndDelete();
}

void RenderGraph::removeConnectionByLinkId(int linkId)
{
    // Find the connection associated with this link ID
    std::string connToRemove;
    for (const auto& [connKey, id] : mLinkIds)
    {
        if (id == linkId)
        {
            connToRemove = connKey;
            break;
        }
    }
    
    if (!connToRemove.empty())
    {
        // Parse connection key to find the actual connection
        size_t arrowPos = connToRemove.find("->");
        if (arrowPos != std::string::npos)
        {
            std::string fromPart = connToRemove.substr(0, arrowPos);
            std::string toPart = connToRemove.substr(arrowPos + 2);
            
            size_t fromDotPos = fromPart.find('.');
            size_t toDotPos = toPart.find('.');
            
            if (fromDotPos != std::string::npos && toDotPos != std::string::npos)
            {
                std::string fromPass = fromPart.substr(0, fromDotPos);
                std::string fromOutput = fromPart.substr(fromDotPos + 1);
                std::string toPass = toPart.substr(0, toDotPos);
                std::string toInput = toPart.substr(toDotPos + 1);
                
                // Remove the connection
                auto it = std::remove_if(mConnections.begin(), mConnections.end(),
                    [&](const RenderGraphConnection& conn) {
                        return conn.fromPass == fromPass && conn.fromOutput == fromOutput &&
                               conn.toPass == toPass && conn.toInput == toInput;
                    });
                
                if (it != mConnections.end())
                {
                    mConnections.erase(it, mConnections.end());
                    mLinkIds.erase(connToRemove);
                    build(); // Rebuild to update dependencies
                }
            }
        }
    }
}

bool RenderGraph::findConnectionDetails(ed::PinId outputPinId, ed::PinId inputPinId, 
                                      std::string& fromPass, std::string& fromOutput, 
                                      std::string& toPass, std::string& toInput)
{
    // Find output pin
    for (const auto& [passName, pinIds] : mOutputPinIds)
    {
        for (size_t i = 0; i < pinIds.size(); ++i)
        {
            if (pinIds[i] == outputPinId.Get())
            {
                fromPass = passName;
                int nodeIdx = findNode(passName);
                if (nodeIdx != -1 && i < mNodes[nodeIdx].pass->getOutputs().size())
                    fromOutput = mNodes[nodeIdx].pass->getOutputs()[i].name;
                break;
            }
        }
        if (!fromPass.empty()) break;
    }
    
    // Find input pin
    for (const auto& [passName, pinIds] : mInputPinIds)
    {
        for (size_t i = 0; i < pinIds.size(); ++i)
        {
            if (pinIds[i] == inputPinId.Get())
            {
                toPass = passName;
                int nodeIdx = findNode(passName);
                if (nodeIdx != -1 && i < mNodes[nodeIdx].pass->getInputs().size())
                    toInput = mNodes[nodeIdx].pass->getInputs()[i].name;
                break;
            }
        }
        if (!toPass.empty()) break;
    }
    
    // Validate connection
    if (fromPass.empty() || fromOutput.empty() || toPass.empty() || toInput.empty())
        return false;
        
    // Check if already exists
    for (const auto& conn : mConnections)
    {
        if (conn.fromPass == fromPass && conn.fromOutput == fromOutput &&
            conn.toPass == toPass && conn.toInput == toInput)
            return false;
    }
    
    return true;
}

void RenderGraph::autoLayoutNodes()
{
    if (!mpEditorContext || mNodes.empty())
        return;
        
    ed::SetCurrentEditor(mpEditorContext);
    
    const float kSpacingX = 300.0f;
    const float kSpacingY = 150.0f;
    const float kStartX = 100.0f;
    const float kStartY = 100.0f;
    
    // Simple grid layout
    for (size_t i = 0; i < mNodes.size(); ++i)
    {
        const auto& nodeName = mNodes[i].name;
        if (mNodeIds.find(nodeName) != mNodeIds.end())
        {
            float x = kStartX + (i % 3) * kSpacingX;
            float y = kStartY + (i / 3) * kSpacingY;
            ed::SetNodePosition(mNodeIds[nodeName], ImVec2(x, y));
        }
    }

    ed::SetCurrentEditor(nullptr);
}
