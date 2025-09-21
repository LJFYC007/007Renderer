#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <array>
#include <imgui.h>
#include <imgui_node_editor.h>

#include "RenderGraph.h"
#include "Core/Pointer.h"

namespace ed = ax::NodeEditor;

class RenderGraphEditor
{
public:
    RenderGraphEditor(ref<Device> pDevice);
    ~RenderGraphEditor();

    // Editor management
    void renderUI();
    void renderNodeEditor();

    // Initialize editor from existing render graph
    void initializeFromRenderGraph(ref<RenderGraph> graph);

    // Access to current valid render graph
    ref<RenderGraph> getCurrentRenderGraph() const { return mpCurrentValidGraph; }

    // Pass management for editor
    void addPass(const std::string& name, ref<RenderPass> pass);
    void removePass(const std::string& name);
    void clearPasses();

    // Connection management for editor
    void addConnection(const std::string& fromPass, const std::string& fromOutput, const std::string& toPass, const std::string& toInput);
    void removeConnection(const std::string& fromPass, const std::string& fromOutput, const std::string& toPass, const std::string& toInput);

    // Scene management
    void setScene(ref<Scene> scene);

private:
    void markDirty() { mIsDirty = true; }
    void rebuild();

    // Node editor utilities
    void initializePassLibrary();
    void drawAddPassControls();
    std::string generateUniqueNodeName(const std::string& baseName) const;
    bool nodeNameExists(const std::string& name) const;

    // Node editor implementation
    void setupNodeEditorStyle();
    void drawNodes();
    void drawConnections();
    void handleNodeEditorInput();

    // Helper functions
    bool findConnectionDetails(
        ed::PinId outputPinId,
        ed::PinId inputPinId,
        std::string& fromPass,
        std::string& fromOutput,
        std::string& toPass,
        std::string& toInput
    );
    bool removeConnectionByLinkId(int linkId);

    inline static constexpr size_t kMaxPassNameLength = 64;

    std::vector<RenderPassDescriptor> mAvailablePasses;
    std::array<char, kMaxPassNameLength> mNewPassNameBuffer{};
    std::string mAddPassErrorMessage;
    int mSelectedPassIndex = -1;
    bool mPendingAddPassPopupReset = false;
    std::string mNodeToFocus;

    struct PinRecord
    {
        std::string pass;
        std::string name;
        RenderDataType type = RenderDataType::Unknown;
        bool isOutput = false;
    };

    std::unordered_map<int, PinRecord> mPinRecords;
    std::unordered_map<int, std::string> mLinkKeyById;

    // Editor state
    std::vector<RenderGraphNode> mEditorNodes;
    std::vector<RenderGraphConnection> mEditorConnections;
    ref<RenderGraph> mpCurrentValidGraph;
    ref<Scene> mpScene;
    ref<Device> mpDevice;
    bool mIsDirty;

    // Node editor context and styling
    ed::EditorContext* mpEditorContext;
    bool mStyleConfigured;
};
