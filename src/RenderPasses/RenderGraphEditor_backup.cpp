#include "RenderGraphEditor.h"
#include "Utils/Logger.h"
#include <algorithm>

RenderGraphEditor::RenderGraphEditor() 
    : mpEditorContext(nullptr), mNextId(1), mStyleConfigured(false), mIsDirty(false)
{
    ed::Config config;
    config.SettingsFile = (std::string(PROJECT_LOG_DIR) + "/editor.json").c_str();
    mpEditorContext = ed::CreateEditor(&config);
    if (!mpEditorContext)
        LOG_ERROR_RETURN("Failed to create node editor context");
}

RenderGraphEditor::~RenderGraphEditor()
{
    if (mpEditorContext)
        ed::DestroyEditor(mpEditorContext);
}

void RenderGraphEditor::addPass(const std::string& name, ref<RenderPass> pass)
{
    mEditorNodes.emplace_back(name, pass);
    markDirty();
}

void RenderGraphEditor::removePass(const std::string& name)
{
    // Remove the node
    auto nodeIt = std::remove_if(mEditorNodes.begin(), mEditorNodes.end(),
        [&name](const RenderGraphNode& node) { return node.name == name; });
    
    if (nodeIt != mEditorNodes.end())
    {
        mEditorNodes.erase(nodeIt, mEditorNodes.end());
        
        // Remove all connections involving this pass
        auto connIt = std::remove_if(mEditorConnections.begin(), mEditorConnections.end(),
            [&name](const RenderGraphConnection& conn) {
                return conn.fromPass == name || conn.toPass == name;
            });
        mEditorConnections.erase(connIt, mEditorConnections.end());
        
        markDirty();
    }
}

void RenderGraphEditor::clearPasses()
{
    mEditorNodes.clear();
    mEditorConnections.clear();
    markDirty();
}

bool RenderGraphEditor::validateCurrentGraph()
{
    // Simplified - just check basic duplicate names
    std::unordered_set<std::string> nodeNames;
    for (const auto& node : mEditorNodes)
    {
        if (nodeNames.count(node.name))
        {
            mLastError = "Duplicate node name: " + node.name;
            return false;
        }
        nodeNames.insert(node.name);
    }
    return true;
}

void RenderGraphEditor::addConnection(const std::string& fromPass, const std::string& fromOutput, 
                                    const std::string& toPass, const std::string& toInput)
{
    mEditorConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
    markDirty();
}

void RenderGraphEditor::removeConnection(const std::string& fromPass, const std::string& fromOutput, 
                                       const std::string& toPass, const std::string& toInput)
{
    auto it = std::remove_if(mEditorConnections.begin(), mEditorConnections.end(),
        [&](const RenderGraphConnection& conn) {
            return conn.fromPass == fromPass && conn.fromOutput == fromOutput &&
                   conn.toPass == toPass && conn.toInput == toInput;
        });
    
    if (it != mEditorConnections.end())
    {
        mEditorConnections.erase(it, mEditorConnections.end());
        markDirty();
    }
}

void RenderGraphEditor::setScene(ref<Scene> scene)
{
    mpScene = scene;
    if (mpCurrentValidGraph)
        mpCurrentValidGraph->setScene(scene);
}

ref<RenderGraph> RenderGraphEditor::buildRenderGraph(ref<Device> pDevice)
{
    if (!mIsDirty && mpCurrentValidGraph)
        return mpCurrentValidGraph;

    // Use RenderGraph::create() for validation
    mLastError.clear();
    mpCurrentValidGraph = RenderGraph::create(pDevice, mEditorNodes, mEditorConnections);
    
    if (mpCurrentValidGraph)
    {
        if (mpScene)
            mpCurrentValidGraph->setScene(mpScene);
        mIsDirty = false;
    }
    else
    {
        mLastError = "Failed to create render graph - check for cycles or invalid connections";
    }
    
    return mpCurrentValidGraph;
}



bool RenderGraphEditor::isDAG(const std::vector<RenderGraphConnection>& connections)
{
    // Build adjacency list from connections
    std::unordered_map<std::string, std::vector<std::string>> adjList;
    std::unordered_set<std::string> allNodes;
    
    // Initialize with all nodes
    for (const auto& node : mEditorNodes)
    {
        allNodes.insert(node.name);
        adjList[node.name] = std::vector<std::string>();
    }
    
    // Build adjacency list from connections
    for (const auto& conn : connections)
    {
        adjList[conn.fromPass].push_back(conn.toPass);
    }
    
    // DFS cycle detection using three colors: WHITE (0), GRAY (1), BLACK (2)
    std::unordered_map<std::string, int> color;
    for (const auto& node : allNodes)
    {
        color[node] = 0; // WHITE
    }
    
    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool
    {
        color[node] = 1; // GRAY
        
        for (const auto& neighbor : adjList[node])
        {
            if (color[neighbor] == 1) // Back edge found - cycle detected
                return false;
            if (color[neighbor] == 0 && !dfs(neighbor))
                return false;
        }
        
        color[node] = 2; // BLACK
        return true;
    };
    
    // Check all nodes for cycles
    for (const auto& node : allNodes)
    {
        if (color[node] == 0)
        {
            if (!dfs(node))
                return false;
        }
    }
    
    return true; // No cycles found - it's a DAG
}

void RenderGraphEditor::removeConnectionsToInput(const std::string& toPass, const std::string& toInput)
{
    // Remove all existing connections to this input
    auto it = std::remove_if(mEditorConnections.begin(), mEditorConnections.end(),
        [&](const RenderGraphConnection& conn) {
            return conn.toPass == toPass && conn.toInput == toInput;
        });
    
    if (it != mEditorConnections.end())
    {
        mEditorConnections.erase(it, mEditorConnections.end());
        mLinkIds.clear(); // Force regeneration of link IDs
        LOG_DEBUG("Removed existing connections to input '{}' in pass '{}'", toInput, toPass);
    }
}

bool RenderGraphEditor::addConnectionSmart(const std::string& fromPass, const std::string& fromOutput, 
                                          const std::string& toPass, const std::string& toInput)
{
    // Step 1: Remove any existing connections to this input (single input constraint)
    removeConnectionsToInput(toPass, toInput);
    
    // Step 2: Create a temporary connection list to test DAG property
    auto tempConnections = mEditorConnections;
    tempConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
    
    // Step 3: Check if the new graph would be a DAG
    if (!isDAG(tempConnections))
    {
        mLastError = "Connection would create a cycle: " + fromPass + " -> " + toPass;
        LOG_WARN("{}", mLastError);
        return false;
    }
    
    // Step 4: Connection is valid, add it to editor state (always update UI state)
    mEditorConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
    mLinkIds.clear(); // Force regeneration
    markDirty();
    
    LOG_DEBUG("Smart connection added: {} ({}) -> {} ({})", 
             fromPass, fromOutput, toPass, toInput);
    return true;
}

void RenderGraphEditor::updateValidGraph(ref<Device> pDevice)
{
    if (!mIsValid)
        return;

    mpCurrentValidGraph = RenderGraph::create(pDevice, mEditorNodes, mEditorConnections);
    if (mpCurrentValidGraph && mpScene)
        mpCurrentValidGraph->setScene(mpScene);
        
    if (mpCurrentValidGraph)
        LOG_DEBUG("Render graph updated successfully with {} nodes and {} connections", 
                 mEditorNodes.size(), mEditorConnections.size());
    else
        LOG_ERROR("Failed to create render graph from editor state");
}

void RenderGraphEditor::initializeFromRenderGraph(ref<RenderGraph> graph)
{
    if (!graph)
    {
        LOG_ERROR("Cannot initialize editor from null render graph");
        return;
    }

    // Clear current editor state
    mEditorNodes.clear();
    mEditorConnections.clear();
    mNodeIds.clear();
    mInputPinIds.clear();
    mOutputPinIds.clear();
    mLinkIds.clear();
    mNodePositions.clear();

    // Copy nodes from the render graph
    mEditorNodes = graph->getNodes();
    
    // Copy connections from the render graph
    mEditorConnections = graph->getConnections();
    
    // Set this graph as the current valid graph
    mpCurrentValidGraph = graph;
    mIsValid = true;
    mIsDirty = false;
    
    LOG_INFO("Editor initialized from render graph with {} nodes and {} connections", 
             mEditorNodes.size(), mEditorConnections.size());
}

void RenderGraphEditor::renderUI()
{    if (GUI::CollapsingHeader("Graph Status"))
    {
        GUI::TextColored(mIsValid ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), 
                        "Render Graph: %s", mIsValid ? "Valid" : "Invalid");
        if (mIsDirty)
            GUI::TextColored(ImVec4(1, 1, 0, 1), "Editor has unsaved changes");
        if (!mLastError.empty())
        {
            GUI::TextColored(ImVec4(1, 0, 0, 1), "Last Error:");
            GUI::TextWrapped("%s", mLastError.c_str());
        }
        GUI::Text("Nodes: %zu | Connections: %zu", mEditorNodes.size(), mEditorConnections.size());
        
        // Show connection validation info
        GUI::Separator();
        GUI::Text("Connection Rules:");
        GUI::BulletText("Types must match (Texture2D <-> Texture2D, Buffer <-> Buffer)");
        GUI::BulletText("Each input can only have one connection (old connections auto-removed)");
        GUI::BulletText("No cycles allowed (DAG structure enforced)");
    }
    
    // Render pass UIs for current valid graph
    if (mpCurrentValidGraph && GUI::CollapsingHeader("Render Passes"))
    {
        for (const auto& node : mpCurrentValidGraph->getNodes())
        {
            if (GUI::CollapsingHeader(node.name.c_str()))
                node.pass->renderUI();
        }
    }
}

void RenderGraphEditor::renderNodeEditor()
{
    setupNodeEditorStyle();    
    
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
    if (GUI::Button("Validate"))
    {
        validateCurrentGraph();
    }
    GUI::SameLine();
    GUI::Text("Nodes: %zu | Connections: %zu", mEditorNodes.size(), mEditorConnections.size());
      // Status indicator
    GUI::SameLine();
    GUI::TextColored(mIsValid ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), 
                    mIsValid ? "Valid" : "Invalid");
    if (mIsDirty)
    {
        GUI::SameLine();
        GUI::TextColored(ImVec4(1, 1, 0, 1), "(Modified)");
    }
    
    // Show last error in node editor if present
    if (!mLastError.empty())
    {
        GUI::Separator();
        GUI::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", mLastError.c_str());
    }
    
    GUI::Separator();
    
    ed::SetCurrentEditor(mpEditorContext);
    
    ed::Begin("Node Editor", ImVec2(0.0f, 0.0f));
    
    // Initialize IDs and position nodes on first frame
    initializeNodeIds();
    static bool sFirstFrame = true;
    if (sFirstFrame)
    {
        for (size_t i = 0; i < mEditorNodes.size(); ++i)
        {
            ImVec2 pos = calculateNodePosition(mEditorNodes[i].name, i);
            ed::SetNodePosition(mNodeIds[mEditorNodes[i].name], pos);
        }
        sFirstFrame = false;
    }
    
    // Draw nodes and connections
    drawNodes();
    drawConnections();
    handleNodeEditorInput();
    
    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void RenderGraphEditor::setupNodeEditorStyle()
{
    if (mStyleConfigured || !mpEditorContext)
        return;
        
    ed::SetCurrentEditor(mpEditorContext);
    
    // Get reference to editor style for direct modification
    auto& style = ed::GetStyle();
    
    // Enhanced Node Editor Styling - Modern Dark Theme
    // Background and Canvas
    style.Colors[ed::StyleColor_Bg]                 = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
    style.Colors[ed::StyleColor_Grid]               = ImVec4(0.12f, 0.12f, 0.18f, 0.6f);
    
    // Nodes - Professional gradient-style appearance
    style.Colors[ed::StyleColor_NodeBg]             = ImVec4(0.18f, 0.20f, 0.25f, 0.95f);
    style.Colors[ed::StyleColor_NodeBorder]         = ImVec4(0.35f, 0.40f, 0.50f, 0.8f);
    style.Colors[ed::StyleColor_HovNodeBorder]      = ImVec4(0.60f, 0.70f, 0.85f, 1.0f);
    style.Colors[ed::StyleColor_SelNodeBorder]      = ImVec4(0.90f, 0.60f, 0.20f, 1.0f);
    
    // Pins - Enhanced visibility and modern colors
    style.Colors[ed::StyleColor_PinRect]            = ImVec4(0.40f, 0.60f, 0.90f, 0.8f);
    style.Colors[ed::StyleColor_PinRectBorder]      = ImVec4(0.60f, 0.80f, 1.0f, 1.0f);
    
    // Links - Use available link border colors for better styling
    style.Colors[ed::StyleColor_HovLinkBorder]      = ImVec4(0.75f, 0.85f, 1.0f, 1.0f);
    style.Colors[ed::StyleColor_SelLinkBorder]      = ImVec4(0.95f, 0.70f, 0.30f, 1.0f);
    style.Colors[ed::StyleColor_HighlightLinkBorder]= ImVec4(0.90f, 0.75f, 0.40f, 1.0f);
    
    // Selection and highlight areas
    style.Colors[ed::StyleColor_NodeSelRect]        = ImVec4(0.90f, 0.60f, 0.20f, 0.3f);
    style.Colors[ed::StyleColor_NodeSelRectBorder]  = ImVec4(0.90f, 0.60f, 0.20f, 0.6f);
    style.Colors[ed::StyleColor_LinkSelRect]        = ImVec4(0.55f, 0.75f, 0.95f, 0.2f);
    style.Colors[ed::StyleColor_LinkSelRectBorder]  = ImVec4(0.55f, 0.75f, 0.95f, 0.5f);
    
    // Flow animation
    style.Colors[ed::StyleColor_Flow]               = ImVec4(0.90f, 0.70f, 0.30f, 1.0f);
    style.Colors[ed::StyleColor_FlowMarker]         = ImVec4(1.0f, 0.80f, 0.40f, 1.0f);
    
    // Groups
    style.Colors[ed::StyleColor_GroupBg]            = ImVec4(0.12f, 0.15f, 0.20f, 0.7f);
    style.Colors[ed::StyleColor_GroupBorder]        = ImVec4(0.45f, 0.55f, 0.70f, 0.6f);
    
    // Style Variables - Enhanced spacing and appearance
    style.NodePadding                   = ImVec4(12, 8, 12, 12);
    style.NodeRounding                  = 6.0f;
    style.NodeBorderWidth               = 1.5f;
    style.HoveredNodeBorderWidth        = 2.5f;
    style.SelectedNodeBorderWidth       = 3.0f;
    style.HoverNodeBorderOffset         = 2.0f;
    style.SelectedNodeBorderOffset      = 2.0f;
    
    // Pin styling
    style.PinRounding                   = 4.0f;
    style.PinBorderWidth                = 1.0f;
    style.PinRadius                     = 6.0f;
    style.PinArrowSize                  = 8.0f;
    style.PinArrowWidth                 = 6.0f;
    
    // Link styling
    style.LinkStrength                  = 150.0f;
    style.FlowMarkerDistance            = 30.0f;
    style.FlowSpeed                     = 150.0f;
    style.FlowDuration                  = 2.0f;
    
    // Group styling
    style.GroupRounding                 = 8.0f;
    style.GroupBorderWidth              = 2.0f;
    
    // Advanced features
    style.HighlightConnectedLinks       = 1.0f;
    style.SnapLinkToPinDir              = 1.0f;
    
    ed::SetCurrentEditor(nullptr);
    mStyleConfigured = true;
    
    LOG_DEBUG("Node editor style configured successfully");
}

void RenderGraphEditor::initializeNodeIds()
{
    for (size_t i = 0; i < mEditorNodes.size(); ++i)
    {
        const auto& node = mEditorNodes[i];
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

ImVec2 RenderGraphEditor::calculateNodePosition(const std::string& nodeName, size_t nodeIndex)
{
    const float kNodeSpacing = 250.0f;
    const float kVerticalSpacing = 150.0f;
    const float kStartX = 50.0f;
    const float kStartY = 50.0f;
    
    // Simple grid layout for now - could be enhanced with dependency-based positioning
    float x = kStartX + (nodeIndex % 3) * kNodeSpacing;
    float y = kStartY + (nodeIndex / 3) * kVerticalSpacing;
    return ImVec2(x, y);
}

void RenderGraphEditor::drawNodes()
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

    // Tooltip state for deferred rendering
    static bool sShowTooltip = false;
    static std::string sTooltipText;
    sShowTooltip = false;

    int uniqueId = 1;
    
    for (const auto& node : mEditorNodes)
    {
        int nodeId = mNodeIds[node.name];
        
        ed::BeginNode(nodeId);
            const auto& inputs = node.pass->getInputs();
            const auto& outputs = node.pass->getOutputs();
            // Node title
            GUI::Text("%s", node.name.c_str());
            
            // Create a two-column layout for inputs and outputs
            GUI::BeginGroup(); // Left column - Inputs
            {
                if (!inputs.empty())
                {
                    GUI::Text("Inputs:");
                    for (size_t i = 0; i < inputs.size(); ++i)
                    {
                        GUI::PushID(uniqueId++);
                        
                        // Input pin
                        ed::BeginPin(mInputPinIds[node.name][i], ed::PinKind::Input);
                            GUI::Text("-> %s", inputs[i].name.c_str());
                        ed::EndPin();
                        
                        // Type info on same line
                        GUI::SameLine(0, 10);
                        GUI::TextDisabled("(%s)", getTypeString(inputs[i].type));
                        
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
                else
                {
                    GUI::TextDisabled("No inputs");
                }
            }
            GUI::EndGroup();
            
            GUI::SameLine();
            GUI::Dummy(ImVec2(50, 0)); // Spacer between columns
            GUI::SameLine();
            
            GUI::BeginGroup(); // Right column - Outputs  
            {
                if (!outputs.empty())
                {
                    GUI::Text("Outputs:");
                    for (size_t i = 0; i < outputs.size(); ++i)
                    {
                        GUI::PushID(uniqueId++);
                        
                        // Type info first
                        GUI::TextDisabled("(%s)", getTypeString(outputs[i].type));
                        GUI::SameLine(0, 10);
                        
                        // Output pin
                        ed::BeginPin(mOutputPinIds[node.name][i], ed::PinKind::Output);
                            GUI::Text("%s ->", outputs[i].name.c_str());
                        ed::EndPin();
                        
                        if (GUI::IsItemHovered())
                        {
                            sShowTooltip = true;
                            sTooltipText = "Output: " + outputs[i].name + 
                                         "\nType: " + getTypeString(outputs[i].type);
                        }
                        GUI::PopID();
                    }
                }
                else
                {
                    GUI::TextDisabled("No outputs");
                }
            }
            GUI::EndGroup();
            
            // Minimum node width
            GUI::Dummy(ImVec2(280, 10));
            
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

void RenderGraphEditor::drawConnections()
{
    // Initialize link IDs if needed
    if (mLinkIds.empty())
    {
        for (const auto& conn : mEditorConnections)
        {
            std::string key = conn.fromPass + "." + conn.fromOutput + "->" + conn.toPass + "." + conn.toInput;
            mLinkIds[key] = mNextId++;
        }
    }
    
    // Draw all connections
    for (const auto& conn : mEditorConnections)
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

int RenderGraphEditor::findOutputPinId(const std::string& passName, const std::string& outputName)
{
    auto pinMapIt = mOutputPinIds.find(passName);
    if (pinMapIt == mOutputPinIds.end())
        return -1;
        
    auto nodeIt = std::find_if(mEditorNodes.begin(), mEditorNodes.end(),
        [&passName](const RenderGraphNode& n) { return n.name == passName; });
    if (nodeIt == mEditorNodes.end())
        return -1;
        
    const auto& outputs = nodeIt->pass->getOutputs();
    for (size_t i = 0; i < outputs.size() && i < pinMapIt->second.size(); ++i)
    {
        if (outputs[i].name == outputName)
            return pinMapIt->second[i];
    }
    
    return -1;
}

int RenderGraphEditor::findInputPinId(const std::string& passName, const std::string& inputName)
{
    auto pinMapIt = mInputPinIds.find(passName);
    if (pinMapIt == mInputPinIds.end())
        return -1;
        
    auto nodeIt = std::find_if(mEditorNodes.begin(), mEditorNodes.end(),
        [&passName](const RenderGraphNode& n) { return n.name == passName; });
    if (nodeIt == mEditorNodes.end())
        return -1;
        
    const auto& inputs = nodeIt->pass->getInputs();
    for (size_t i = 0; i < inputs.size() && i < pinMapIt->second.size(); ++i)
    {
        if (inputs[i].name == inputName)
            return pinMapIt->second[i];
    }
    
    return -1;
}

void RenderGraphEditor::handleNodeEditorInput()
{    // Handle new connection creation
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
                        if (addConnectionSmart(fromPass, fromOutput, toPass, toInput))
                        {
                            LOG_DEBUG("Smart connection created: {} ({}) -> {} ({})", 
                                    fromPass, fromOutput, toPass, toInput);
                        }
                        else
                        {
                            LOG_WARN("Smart connection failed: {}", mLastError);
                        }
                    }
                }
                else
                {
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                    if (!mLastError.empty())
                        LOG_DEBUG("Connection rejected: {}", mLastError);
                    else
                        LOG_DEBUG("Connection rejected: Invalid connection attempt");
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
                if (removeConnectionByLinkId(linkId.Get()))
                {
                    LOG_DEBUG("Connection deleted successfully");
                }
                else
                {
                    LOG_WARN("Failed to remove connection by link ID: {}", linkId.Get());
                }
            }
        }
        
        // Handle node deletion (optional - if you want to support deleting nodes)
        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId))
        {
            if (ed::AcceptDeletedItem())
            {
                // Find and remove the node
                std::string nodeToRemove;
                for (const auto& [nodeName, id] : mNodeIds)
                {
                    if (id == nodeId.Get())
                    {
                        nodeToRemove = nodeName;
                        break;
                    }
                }
                
                if (!nodeToRemove.empty())
                {
                    removePass(nodeToRemove);
                    LOG_DEBUG("Node deleted: {}", nodeToRemove);
                }
            }
        }
    }
    ed::EndDelete();
}

bool RenderGraphEditor::removeConnectionByLinkId(int linkId)
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
    
    if (connToRemove.empty())
    {
        LOG_WARN("Could not find connection for link ID: {}", linkId);
        return false;
    }
    
    // Parse connection key to find the actual connection
    size_t arrowPos = connToRemove.find("->");
    if (arrowPos == std::string::npos)
    {
        LOG_WARN("Invalid connection key format: {}", connToRemove);
        return false;
    }
    
    std::string fromPart = connToRemove.substr(0, arrowPos);
    std::string toPart = connToRemove.substr(arrowPos + 2);
    
    size_t fromDotPos = fromPart.find('.');
    size_t toDotPos = toPart.find('.');
    
    if (fromDotPos == std::string::npos || toDotPos == std::string::npos)
    {
        LOG_WARN("Invalid connection key parts: {}", connToRemove);
        return false;
    }
    
    std::string fromPass = fromPart.substr(0, fromDotPos);
    std::string fromOutput = fromPart.substr(fromDotPos + 1);
    std::string toPass = toPart.substr(0, toDotPos);
    std::string toInput = toPart.substr(toDotPos + 1);
    
    // Remove the connection
    auto it = std::remove_if(mEditorConnections.begin(), mEditorConnections.end(),
        [&](const RenderGraphConnection& conn) {
            return conn.fromPass == fromPass && conn.fromOutput == fromOutput &&
                   conn.toPass == toPass && conn.toInput == toInput;
        });
    
    if (it != mEditorConnections.end())
    {
        mEditorConnections.erase(it, mEditorConnections.end());
        mLinkIds.erase(connToRemove);
        markDirty();
        return true;
    }
    else
    {
        LOG_WARN("Connection not found in editor state: {}", connToRemove);
        return false;
    }
}

bool RenderGraphEditor::findConnectionDetails(ed::PinId outputPinId, ed::PinId inputPinId, 
                                            std::string& fromPass, std::string& fromOutput, 
                                            std::string& toPass, std::string& toInput)
{
    // Clear output parameters
    fromPass.clear();
    fromOutput.clear();
    toPass.clear();
    toInput.clear();
    
    RenderDataType outputType = RenderDataType::Unknown;
    RenderDataType inputType = RenderDataType::Unknown;
    
    // Find output pin
    bool foundOutput = false;
    for (const auto& [passName, pinIds] : mOutputPinIds)
    {
        for (size_t i = 0; i < pinIds.size(); ++i)
        {
            if (pinIds[i] == outputPinId.Get())
            {
                fromPass = passName;
                auto nodeIt = std::find_if(mEditorNodes.begin(), mEditorNodes.end(),
                    [&passName](const RenderGraphNode& n) { return n.name == passName; });
                if (nodeIt != mEditorNodes.end() && i < nodeIt->pass->getOutputs().size())
                {
                    fromOutput = nodeIt->pass->getOutputs()[i].name;
                    outputType = nodeIt->pass->getOutputs()[i].type;
                    foundOutput = true;
                }
                break;
            }
        }
        if (foundOutput) break;
    }
    
    // Find input pin
    bool foundInput = false;
    for (const auto& [passName, pinIds] : mInputPinIds)
    {
        for (size_t i = 0; i < pinIds.size(); ++i)
        {
            if (pinIds[i] == inputPinId.Get())
            {
                toPass = passName;
                auto nodeIt = std::find_if(mEditorNodes.begin(), mEditorNodes.end(),
                    [&passName](const RenderGraphNode& n) { return n.name == passName; });
                if (nodeIt != mEditorNodes.end() && i < nodeIt->pass->getInputs().size())
                {
                    toInput = nodeIt->pass->getInputs()[i].name;
                    inputType = nodeIt->pass->getInputs()[i].type;
                    foundInput = true;
                }
                break;
            }
        }
        if (foundInput) break;
    }
    
    // Check type compatibility
    if (foundOutput && foundInput && outputType != inputType)
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
        
        mLastError = "Type mismatch: Output '" + fromOutput + "' (" + getTypeString(outputType) + 
                    ") cannot connect to input '" + toInput + "' (" + getTypeString(inputType) + ")";
        LOG_WARN("{}", mLastError);
        return false;
    }
    
    return foundOutput && foundInput;
}

void RenderGraphEditor::autoLayoutNodes()
{
    if (mEditorNodes.empty())
        return;
        
    ed::SetCurrentEditor(mpEditorContext);
    
    // Simple grid layout
    const float kNodeSpacing = 300.0f;
    const float kVerticalSpacing = 200.0f;
    const float kStartX = 100.0f;
    const float kStartY = 100.0f;
    
    for (size_t i = 0; i < mEditorNodes.size(); ++i)
    {
        const auto& node = mEditorNodes[i];
        if (mNodeIds.find(node.name) != mNodeIds.end())
        {
            float x = kStartX + (i % 4) * kNodeSpacing;
            float y = kStartY + (i / 4) * kVerticalSpacing;
            ed::SetNodePosition(mNodeIds[node.name], ImVec2(x, y));
        }
    }
    
    ed::SetCurrentEditor(nullptr);
    LOG_DEBUG("Auto layout applied to {} nodes", mEditorNodes.size());
}
