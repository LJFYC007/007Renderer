#include <algorithm>
#include <cstring>

#include "RenderGraphEditor.h"
#include "Utils/Logger.h"
#include <imgui_internal.h>

namespace
{
inline int hashId(const std::string& key)
{
    return static_cast<int>(ImHashStr(key.c_str()));
}

inline ed::NodeId makeNodeId(const std::string& pass)
{
    return ed::NodeId(hashId("node|" + pass));
}

inline ed::PinId makePinId(const std::string& pass, const std::string& pin, bool isOutput)
{
    return ed::PinId(hashId(std::string(isOutput ? "pin|out|" : "pin|in|") + pass + "|" + pin));
}

inline ed::LinkId makeLinkId(const std::string& key)
{
    return ed::LinkId(hashId("link|" + key));
}

inline std::string makeConnectionKey(const RenderGraphConnection& conn)
{
    return conn.fromPass + "." + conn.fromOutput + "->" + conn.toPass + "." + conn.toInput;
}

inline bool parseConnectionKey(const std::string& key, std::string& fromPass, std::string& fromOutput, std::string& toPass, std::string& toInput)
{
    const auto arrow = key.find("->");
    if (arrow == std::string::npos)
        return false;
    const std::string fromPart = key.substr(0, arrow);
    const std::string toPart = key.substr(arrow + 2);
    const auto fromDot = fromPart.find('.');
    const auto toDot = toPart.find('.');
    if (fromDot == std::string::npos || toDot == std::string::npos)
        return false;
    fromPass = fromPart.substr(0, fromDot);
    fromOutput = fromPart.substr(fromDot + 1);
    toPass = toPart.substr(0, toDot);
    toInput = toPart.substr(toDot + 1);
    return true;
}
} // namespace

RenderGraphEditor::RenderGraphEditor(ref<Device> pDevice) : mpDevice(pDevice), mpEditorContext(nullptr), mStyleConfigured(false), mIsDirty(false)
{
    mNewPassNameBuffer.fill('\0');
    initializePassLibrary();

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

void RenderGraphEditor::initializePassLibrary()
{
    const auto& registeredPasses = RenderPassRegistry::getRegisteredPasses();
    mAvailablePasses.assign(registeredPasses.begin(), registeredPasses.end());
}

bool RenderGraphEditor::nodeNameExists(const std::string& name) const
{
    return std::any_of(mEditorNodes.begin(), mEditorNodes.end(), [&name](const RenderGraphNode& node) { return node.name == name; });
}

std::string RenderGraphEditor::generateUniqueNodeName(const std::string& baseName) const
{
    if (!nodeNameExists(baseName))
        return baseName;

    int suffix = 2;
    std::string candidate;
    do
    {
        candidate = baseName + std::to_string(suffix++);
    } while (nodeNameExists(candidate));
    return candidate;
}

void RenderGraphEditor::drawAddPassControls()
{
    initializePassLibrary();

    if (GUI::Button("Add Pass"))
    {
        ImGui::OpenPopup("RenderGraphEditor.AddPass");
        mPendingAddPassPopupReset = true;
    }

    if (ImGui::BeginPopup("RenderGraphEditor.AddPass"))
    {
        if (mPendingAddPassPopupReset)
        {
            mPendingAddPassPopupReset = false;
            mSelectedPassIndex = -1;
            mAddPassErrorMessage.clear();
            mNewPassNameBuffer.fill('\0');
        }

        GUI::Text("Select a render pass to insert");
        GUI::Separator();

        const bool hasPasses = !mAvailablePasses.empty();

        if (!hasPasses)
        {
            GUI::TextDisabled("No registered render passes available.");
        }
        else
        {
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            if (GUI::BeginChild("PassList", ImVec2(contentWidth, 180.0f)))
            {
                for (size_t i = 0; i < mAvailablePasses.size(); ++i)
                {
                    const auto& descriptor = mAvailablePasses[i];
                    const bool isSelected = static_cast<int>(i) == mSelectedPassIndex;
                    if (ImGui::Selectable(descriptor.displayName.c_str(), isSelected))
                    {
                        mSelectedPassIndex = static_cast<int>(i);
                        const std::string defaultName = generateUniqueNodeName(descriptor.displayName);
                        mNewPassNameBuffer.fill('\0');
                        std::strncpy(mNewPassNameBuffer.data(), defaultName.c_str(), mNewPassNameBuffer.size() - 1);
                        mAddPassErrorMessage.clear();
                    }
                }
            }
            GUI::EndChild();

            if (mSelectedPassIndex >= 0)
            {
                ImGui::Spacing();
                GUI::Text("%s", mAvailablePasses[static_cast<size_t>(mSelectedPassIndex)].description.c_str());
            }

            ImGui::Spacing();
            if (ImGui::InputText("Node Name", mNewPassNameBuffer.data(), static_cast<int>(mNewPassNameBuffer.size())))
                mAddPassErrorMessage.clear();
        }

        const bool canCreate = hasPasses && mSelectedPassIndex >= 0 && mNewPassNameBuffer[0] != '\0';

        if (!mAddPassErrorMessage.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", mAddPassErrorMessage.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        if (!canCreate)
            ImGui::BeginDisabled();

        if (GUI::Button("Create"))
        {
            std::string newName = mNewPassNameBuffer.data();
            if (newName.empty())
            {
                mAddPassErrorMessage = "Node name cannot be empty.";
            }
            else if (nodeNameExists(newName))
            {
                mAddPassErrorMessage = "A node with this name already exists.";
            }
            else if (!hasPasses || mSelectedPassIndex < 0 || static_cast<size_t>(mSelectedPassIndex) >= mAvailablePasses.size())
            {
                mAddPassErrorMessage = "Select a render pass to add.";
            }
            else
            {
                auto& descriptor = mAvailablePasses[static_cast<size_t>(mSelectedPassIndex)];
                ref<RenderPass> newPass = descriptor.factory(mpDevice);
                if (!newPass)
                {
                    mAddPassErrorMessage = "Failed to construct the requested render pass.";
                }
                else
                {
                    addPass(newName, newPass);
                    mNodeToFocus = newName;
                    mAddPassErrorMessage.clear();
                    ImGui::CloseCurrentPopup();
                    mPendingAddPassPopupReset = true;
                }
            }
        }

        if (!canCreate)
            ImGui::EndDisabled();

        GUI::SameLine();
        if (GUI::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void RenderGraphEditor::addPass(const std::string& name, ref<RenderPass> pass)
{
    if (mpScene)
        pass->setScene(mpScene);

    mEditorNodes.emplace_back(name, pass);
    markDirty();
}

void RenderGraphEditor::removePass(const std::string& name)
{
    // Remove the node
    auto nodeIt = std::remove_if(mEditorNodes.begin(), mEditorNodes.end(), [&name](const RenderGraphNode& node) { return node.name == name; });

    if (nodeIt != mEditorNodes.end())
    {
        mEditorNodes.erase(nodeIt, mEditorNodes.end());

        // Remove all connections involving this pass
        auto connIt = std::remove_if(
            mEditorConnections.begin(),
            mEditorConnections.end(),
            [&name](const RenderGraphConnection& conn) { return conn.fromPass == name || conn.toPass == name; }
        );
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

void RenderGraphEditor::addConnection(
    const std::string& fromPass,
    const std::string& fromOutput,
    const std::string& toPass,
    const std::string& toInput
)
{
    mEditorConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
    markDirty();
}

void RenderGraphEditor::removeConnection(
    const std::string& fromPass,
    const std::string& fromOutput,
    const std::string& toPass,
    const std::string& toInput
)
{
    auto it = std::remove_if(
        mEditorConnections.begin(),
        mEditorConnections.end(),
        [&](const RenderGraphConnection& conn)
        { return conn.fromPass == fromPass && conn.fromOutput == fromOutput && conn.toPass == toPass && conn.toInput == toInput; }
    );

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
    if (!graph)
        return;

    // Clear and copy data
    mEditorNodes = graph->getNodes();
    mEditorConnections = graph->getConnections();
    mpCurrentValidGraph = graph;
    mIsDirty = false;

    // Clear cached UI state to force regeneration
    mPinRecords.clear();
    mLinkKeyById.clear();
    mNodeToFocus.clear();

    LOG_INFO("Editor initialized from render graph with {} nodes and {} connections", mEditorNodes.size(), mEditorConnections.size());
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
    drawAddPassControls();

    GUI::Separator();

    ed::SetCurrentEditor(mpEditorContext);
    ed::Begin("Node Editor", ImVec2(0.0f, 0.0f));

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
    auto& style = ed::GetStyle();

    struct ColorDef
    {
        ed::StyleColor slot;
        ImVec4 color;
    };

    const ColorDef colors[] = {
        {ed::StyleColor_Bg, ImVec4(0.08f, 0.08f, 0.12f, 1.0f)},
        {ed::StyleColor_Grid, ImVec4(0.12f, 0.12f, 0.18f, 0.6f)},
        {ed::StyleColor_NodeBg, ImVec4(0.18f, 0.20f, 0.25f, 0.95f)},
        {ed::StyleColor_NodeBorder, ImVec4(0.35f, 0.40f, 0.50f, 0.8f)},
        {ed::StyleColor_HovNodeBorder, ImVec4(0.60f, 0.70f, 0.85f, 1.0f)},
        {ed::StyleColor_SelNodeBorder, ImVec4(0.90f, 0.60f, 0.20f, 1.0f)},
        {ed::StyleColor_PinRect, ImVec4(0.40f, 0.60f, 0.90f, 0.8f)},
        {ed::StyleColor_PinRectBorder, ImVec4(0.60f, 0.80f, 1.0f, 1.0f)},
        {ed::StyleColor_HovLinkBorder, ImVec4(0.75f, 0.85f, 1.0f, 1.0f)},
        {ed::StyleColor_SelLinkBorder, ImVec4(0.95f, 0.70f, 0.30f, 1.0f)},
        {ed::StyleColor_HighlightLinkBorder, ImVec4(0.90f, 0.75f, 0.40f, 1.0f)},
        {ed::StyleColor_NodeSelRect, ImVec4(0.90f, 0.60f, 0.20f, 0.3f)},
        {ed::StyleColor_NodeSelRectBorder, ImVec4(0.90f, 0.60f, 0.20f, 0.6f)},
        {ed::StyleColor_LinkSelRect, ImVec4(0.55f, 0.75f, 0.95f, 0.2f)},
        {ed::StyleColor_LinkSelRectBorder, ImVec4(0.55f, 0.75f, 0.95f, 0.5f)},
        {ed::StyleColor_Flow, ImVec4(0.90f, 0.70f, 0.30f, 1.0f)},
        {ed::StyleColor_FlowMarker, ImVec4(1.0f, 0.80f, 0.40f, 1.0f)},
        {ed::StyleColor_GroupBg, ImVec4(0.12f, 0.15f, 0.20f, 0.7f)},
        {ed::StyleColor_GroupBorder, ImVec4(0.45f, 0.55f, 0.70f, 0.6f)}
    };
    for (const auto& def : colors)
        style.Colors[def.slot] = def.color;

    style.NodePadding = ImVec4(12, 8, 12, 12);
    style.NodeRounding = 6.0f;
    style.NodeBorderWidth = 1.5f;
    style.HoveredNodeBorderWidth = 2.5f;
    style.SelectedNodeBorderWidth = 3.0f;
    style.HoverNodeBorderOffset = 2.0f;
    style.SelectedNodeBorderOffset = 2.0f;

    style.PinRounding = 4.0f;
    style.PinBorderWidth = 1.0f;
    style.PinRadius = 6.0f;
    style.PinArrowSize = 8.0f;
    style.PinArrowWidth = 6.0f;

    style.LinkStrength = 150.0f;
    style.FlowMarkerDistance = 30.0f;
    style.FlowSpeed = 150.0f;
    style.FlowDuration = 2.0f;

    style.GroupRounding = 8.0f;
    style.GroupBorderWidth = 2.0f;
    style.HighlightConnectedLinks = 1.0f;
    style.SnapLinkToPinDir = 1.0f;

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
    {
        mpCurrentValidGraph = renderGraph;
        mpCurrentValidGraph->setScene(mpScene);
    }
}

void RenderGraphEditor::drawNodes()
{
    struct TooltipState
    {
        bool show = false;
        std::string text;
    } tooltip;

    mPinRecords.clear();
    GUI::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    for (const auto& node : mEditorNodes)
    {
        const auto& inputs = node.pass->getInputs();
        const auto& outputs = node.pass->getOutputs();

        GUI::PushID(node.name.c_str());
        ed::BeginNode(makeNodeId(node.name));
        GUI::Text("%s", node.name.c_str());

        GUI::BeginGroup();
        if (!inputs.empty())
        {
            GUI::Text("Inputs:");
            GUI::PushID("Inputs");
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                GUI::PushID(static_cast<int>(i));
                const auto& pin = inputs[i];
                const ed::PinId pinId = makePinId(node.name, pin.name, false);
                mPinRecords[pinId.Get()] = PinRecord{node.name, pin.name, pin.type, false};

                ed::BeginPin(pinId, ed::PinKind::Input);
                GUI::Text("-> %s", pin.name.c_str());
                ed::EndPin();

                if (GUI::IsItemHovered())
                {
                    tooltip.show = true;
                    tooltip.text = "Input: " + pin.name + "Optional: " + (pin.optional ? "Yes" : "No");
                }

                GUI::PopID();
            }
            GUI::PopID();
        }
        else
        {
            GUI::TextDisabled("No inputs");
        }
        GUI::EndGroup();

        GUI::SameLine();
        GUI::Dummy(ImVec2(50, 0));
        GUI::SameLine();

        GUI::BeginGroup();
        if (!outputs.empty())
        {
            GUI::Text("Outputs:");
            GUI::PushID("Outputs");
            for (size_t i = 0; i < outputs.size(); ++i)
            {
                GUI::PushID(static_cast<int>(i));
                const auto& pin = outputs[i];
                const ed::PinId pinId = makePinId(node.name, pin.name, true);
                mPinRecords[pinId.Get()] = PinRecord{node.name, pin.name, pin.type, true};

                ed::BeginPin(pinId, ed::PinKind::Output);
                GUI::Text("%s ->\n", pin.name.c_str());
                ed::EndPin();

                if (GUI::IsItemHovered())
                {
                    tooltip.show = true;
                    tooltip.text = "Output: " + pin.name;
                }

                GUI::PopID();
            }
            GUI::PopID();
        }
        else
        {
            GUI::TextDisabled("No outputs");
        }
        GUI::EndGroup();

        GUI::Dummy(ImVec2(280, 10));
        ed::EndNode();
        GUI::PopID();
    }

    GUI::PopStyleColor();

    if (!mNodeToFocus.empty())
    {
        ed::SelectNode(makeNodeId(mNodeToFocus));
        ed::NavigateToSelection(false);
        mNodeToFocus.clear();
    }

    if (tooltip.show)
    {
        ed::Suspend();
        GUI::SetTooltip("%s", tooltip.text.c_str());
        ed::Resume();
    }
}

void RenderGraphEditor::drawConnections()
{
    mLinkKeyById.clear();
    for (const auto& conn : mEditorConnections)
    {
        const std::string key = makeConnectionKey(conn);
        const ed::PinId fromPin = makePinId(conn.fromPass, conn.fromOutput, true);
        const ed::PinId toPin = makePinId(conn.toPass, conn.toInput, false);
        if (!mPinRecords.count(fromPin.Get()) || !mPinRecords.count(toPin.Get()))
            continue;
        const ed::LinkId link = makeLinkId(key);
        mLinkKeyById[link.Get()] = key;
        ed::Link(link, fromPin, toPin);
    }
}

void RenderGraphEditor::handleNodeEditorInput()
{
    if (ed::BeginCreate())
    {
        ed::PinId startPin, endPin;
        if (ed::QueryNewLink(&startPin, &endPin) && startPin && endPin)
        {
            std::string fromPass, fromOutput, toPass, toInput;
            const bool forward = findConnectionDetails(startPin, endPin, fromPass, fromOutput, toPass, toInput);
            const bool backward = !forward && findConnectionDetails(endPin, startPin, fromPass, fromOutput, toPass, toInput);

            if (forward || backward)
            {
                if (ed::AcceptNewItem())
                {
                    auto it = std::remove_if(
                        mEditorConnections.begin(),
                        mEditorConnections.end(),
                        [&](const RenderGraphConnection& conn) { return conn.toPass == toPass && conn.toInput == toInput; }
                    );
                    mEditorConnections.erase(it, mEditorConnections.end());

                    mEditorConnections.emplace_back(fromPass, fromOutput, toPass, toInput);
                    mLinkKeyById.clear();
                    markDirty();
                    LOG_DEBUG("Connection created: {} ({}) -> {} ({})", fromPass, fromOutput, toPass, toInput);
                }
            }
            else
            {
                ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId))
            if (ed::AcceptDeletedItem())
            {
                if (removeConnectionByLinkId(linkId.Get()))
                    LOG_DEBUG("Connection deleted successfully");
                else
                    LOG_WARN("Failed to remove connection by link ID: {}", linkId.Get());
            }

        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId))
        {
            if (!ed::AcceptDeletedItem())
                continue;

            std::string nodeToRemove;
            for (const auto& node : mEditorNodes)
            {
                if (makeNodeId(node.name).Get() == nodeId.Get())
                {
                    nodeToRemove = node.name;
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
    ed::EndDelete();
}

bool RenderGraphEditor::removeConnectionByLinkId(int linkId)
{
    auto keyIt = mLinkKeyById.find(linkId);
    if (keyIt == mLinkKeyById.end())
    {
        LOG_WARN("Could not find connection for link ID: {}", linkId);
        return false;
    }

    std::string fromPass, fromOutput, toPass, toInput;
    if (!parseConnectionKey(keyIt->second, fromPass, fromOutput, toPass, toInput))
    {
        LOG_WARN("Invalid connection key format: {}", keyIt->second);
        mLinkKeyById.erase(keyIt);
        return false;
    }

    auto it = std::remove_if(
        mEditorConnections.begin(),
        mEditorConnections.end(),
        [&](const RenderGraphConnection& conn)
        { return conn.fromPass == fromPass && conn.fromOutput == fromOutput && conn.toPass == toPass && conn.toInput == toInput; }
    );

    if (it != mEditorConnections.end())
    {
        mEditorConnections.erase(it, mEditorConnections.end());
        mLinkKeyById.erase(keyIt);
        markDirty();
        return true;
    }

    LOG_WARN("Connection not found in editor state: {}", keyIt->second);
    mLinkKeyById.erase(keyIt);
    return false;
}

bool RenderGraphEditor::findConnectionDetails(
    ed::PinId pinA,
    ed::PinId pinB,
    std::string& fromPass,
    std::string& fromOutput,
    std::string& toPass,
    std::string& toInput
)
{
    auto recordA = mPinRecords.find(pinA.Get());
    auto recordB = mPinRecords.find(pinB.Get());
    if (recordA == mPinRecords.end() || recordB == mPinRecords.end())
        return false;

    const PinRecord& a = recordA->second;
    const PinRecord& b = recordB->second;
    if (a.type != b.type || a.isOutput == b.isOutput)
        return false;

    const PinRecord& output = a.isOutput ? a : b;
    const PinRecord& input = a.isOutput ? b : a;

    fromPass = output.pass;
    fromOutput = output.name;
    toPass = input.pass;
    toInput = input.name;
    return true;
}
