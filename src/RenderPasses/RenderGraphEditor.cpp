#include <algorithm>

#include "RenderGraphEditor.h"
#include "Utils/Logger.h"

RenderGraphEditor::RenderGraphEditor(ref<Device> pDevice) 
    : mpDevice(pDevice), mpEditorContext(nullptr), mNextId(1), mStyleConfigured(false), mIsDirty(false)
{
    ed::Config config;
    config.SettingsFile = "editor.json";
    mpEditorContext = ed::CreateEditor(&config);
    if (!mpEditorContext)
        LOG_ERROR_RETURN("Failed to create node editor context");
    
    LOG_INFO("Render graph editor initialized successfully");
}

RenderGraphEditor::~RenderGraphEditor()
{
    GUI::SaveIniSettingsToDisk(GUI::GetIO().IniFilename);
    ed::SetCurrentEditor(nullptr);
    ed::DestroyEditor(mpEditorContext);
    LOG_DEBUG("Render graph editor destroyed with settings saved");
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

void RenderGraphEditor::initializeFromRenderGraph(ref<RenderGraph> graph)
{
    if (!graph) return;

    // Clear and copy data
    mEditorNodes = graph->getNodes();
    mEditorConnections = graph->getConnections();
    mpCurrentValidGraph = graph;
    mIsDirty = false;
    
    // Clear UI state to force regeneration
    mNodeIds.clear();
    mInputPinIds.clear();
    mOutputPinIds.clear();
    mLinkIds.clear();
    
    LOG_INFO("Editor initialized from render graph with {} nodes and {} connections", 
             mEditorNodes.size(), mEditorConnections.size());
}

void RenderGraphEditor::renderUI()
{
    // Render individual pass UIs
    for (const auto& node : mpCurrentValidGraph->getNodes())
        if (GUI::CollapsingHeader(node.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            node.pass->renderUI();
}

void RenderGraphEditor::renderNodeEditor()
{
    setupNodeEditorStyle();    
    rebuild();
    
    // Control panel
    GUI::Text("Nodes: %zu | Connections: %zu", mEditorNodes.size(), mEditorConnections.size());
    
    GUI::Separator();
    
    ed::SetCurrentEditor(mpEditorContext);
    ed::Begin("Node Editor", ImVec2(0.0f, 0.0f));
    
    // Initialize IDs - let editor use saved positions from config file
    initializeNodeIds();
    
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

void RenderGraphEditor::rebuild() 
{
    if (mIsDirty == false)
        return;
    mIsDirty = false; 
    ref<RenderGraph> renderGraph = RenderGraph::create(mpDevice, mEditorNodes, mEditorConnections);
    if (renderGraph)
        mpCurrentValidGraph = renderGraph;    
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

void RenderGraphEditor::drawNodes()
{
    // Tooltip state for deferred rendering
    static bool sShowTooltip = false;
    static std::string sTooltipText;
    sShowTooltip = false;

    int uniqueId = 1;
    
    // Set pure white color for all text in nodes
    GUI::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
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
                        
                        if (GUI::IsItemHovered())
                        {
                            sShowTooltip = true;
                            sTooltipText = "Input: " + inputs[i].name + "\nOptional: " + (inputs[i].optional ? "Yes" : "No");
                        }
                        GUI::PopID();
                    }
                }
                else
                    GUI::TextDisabled("No inputs");
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
                        // Output pin
                        ed::BeginPin(mOutputPinIds[node.name][i], ed::PinKind::Output);
                            GUI::Text("%s ->\n", outputs[i].name.c_str());
                        ed::EndPin();
                        
                        if (GUI::IsItemHovered())
                        {
                            sShowTooltip = true;
                            sTooltipText = "Output: " + outputs[i].name;
                        }
                        GUI::PopID();
                    }
                }
                else
                    GUI::TextDisabled("No outputs");
            }
            GUI::EndGroup();
            
            // Minimum node width
            GUI::Dummy(ImVec2(280, 10));
            
        ed::EndNode();
    }
    // Restore original text color
    GUI::PopStyleColor();
    
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
{
    // Handle new connection creation
    if (ed::BeginCreate())
    {
        ed::PinId startPin, endPin;
        if (ed::QueryNewLink(&startPin, &endPin))
        {
            if (startPin && endPin)
            {
                std::string fromPass, fromOutput, toPass, toInput;
                bool validConnection = false;
                
                // Try connection as output->input first
                if (findConnectionDetails(startPin, endPin, fromPass, fromOutput, toPass, toInput))
                    validConnection = true;
                // If that failed, try input->output (reverse direction)  
                else if (findConnectionDetails(endPin, startPin, fromPass, fromOutput, toPass, toInput))
                    validConnection = true;
                
                if (validConnection)
                {
                    if (ed::AcceptNewItem())
                    {
                        // Remove existing connections to this input first
                        auto it = std::remove_if(mEditorConnections.begin(), mEditorConnections.end(),
                            [&](const RenderGraphConnection& conn) {
                                return conn.toPass == toPass && conn.toInput == toInput;
                            });
                        mEditorConnections.erase(it, mEditorConnections.end());
                        
                        // Add new connection
                        mEditorConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
                        mLinkIds.clear();
                        markDirty();
                        LOG_DEBUG("Connection created: {} ({}) -> {} ({})", 
                                fromPass, fromOutput, toPass, toInput);
                    }
                }
                else
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
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
                    LOG_DEBUG("Connection deleted successfully");
                else
                    LOG_WARN("Failed to remove connection by link ID: {}", linkId.Get());
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

bool RenderGraphEditor::findConnectionDetails(ed::PinId pin1Id, ed::PinId pin2Id, 
                                            std::string& fromPass, std::string& fromOutput, 
                                            std::string& toPass, std::string& toInput)
{
    // Pin information structures
    struct PinInfo {
        std::string passName;
        std::string pinName;
        RenderDataType dataType = RenderDataType::Unknown;
        bool isOutput = false;
        bool found = false;
    };
    
    PinInfo pin1Info, pin2Info;
    
    // Helper lambda to find pin information
    auto findPinInfo = [&](ed::PinId pinId, PinInfo& info)
    {
        // Try to find as output pin
        for (const auto& [passName, pinIds] : mOutputPinIds)
        {
            for (size_t i = 0; i < pinIds.size(); ++i)
            {
                if (pinIds[i] == pinId.Get())
                {
                    info.passName = passName;
                    auto nodeIt = std::find_if(mEditorNodes.begin(), mEditorNodes.end(),
                        [&passName](const RenderGraphNode& n) { return n.name == passName; });
                    if (nodeIt != mEditorNodes.end() && i < nodeIt->pass->getOutputs().size())
                    {
                        info.pinName = nodeIt->pass->getOutputs()[i].name;
                        info.dataType = nodeIt->pass->getOutputs()[i].type;
                        info.isOutput = true;
                        info.found = true;
                        return;
                    }
                }
            }
        }
        
        // Try to find as input pin
        for (const auto& [passName, pinIds] : mInputPinIds)
        {
            for (size_t i = 0; i < pinIds.size(); ++i)
            {
                if (pinIds[i] == pinId.Get())
                {
                    info.passName = passName;
                    auto nodeIt = std::find_if(mEditorNodes.begin(), mEditorNodes.end(),
                        [&passName](const RenderGraphNode& n) { return n.name == passName; });
                    if (nodeIt != mEditorNodes.end() && i < nodeIt->pass->getInputs().size())
                    {
                        info.pinName = nodeIt->pass->getInputs()[i].name;
                        info.dataType = nodeIt->pass->getInputs()[i].type;
                        info.isOutput = false;
                        info.found = true;
                        return;
                    }
                }
            }
        }
    };
    
    // Find information for both pins
    findPinInfo(pin1Id, pin1Info);
    findPinInfo(pin2Id, pin2Info);
    
    // Check if we found both pins and they have compatible types
    if (!pin1Info.found || !pin2Info.found || pin1Info.dataType != pin2Info.dataType)
        return false;
    
    // Determine which pin is the output and which is the input
    PinInfo* outputPin = nullptr;
    PinInfo* inputPin = nullptr;
    
    if (pin1Info.isOutput && !pin2Info.isOutput)
    {
        outputPin = &pin1Info;
        inputPin = &pin2Info;
    }
    else if (!pin1Info.isOutput && pin2Info.isOutput)
    {
        outputPin = &pin2Info;
        inputPin = &pin1Info;
    }
    else
    {
        // Both are outputs or both are inputs - invalid connection
        return false;
    }
    
    // Set connection details
    fromPass = outputPin->passName;
    fromOutput = outputPin->pinName;
    toPass = inputPin->passName;
    toInput = inputPin->pinName;
    
    return true;
}
