#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <imgui.h>
#include <imgui_node_editor.h>

#include "RenderGraph.h"
#include "Core/Pointer.h"

namespace ed = ax::NodeEditor;

class RenderGraphEditor
{
public:
    RenderGraphEditor();
    ~RenderGraphEditor();

    // Editor management
    void renderUI();
    void renderNodeEditor();

    // Graph management - returns nullptr if current editor state is invalid
    ref<RenderGraph> buildRenderGraph(ref<Device> pDevice);
    
    // Initialize editor from existing render graph
    void initializeFromRenderGraph(ref<RenderGraph> graph);
      // Access to current valid render graph
    ref<RenderGraph> getCurrentRenderGraph() const { return mpCurrentValidGraph; }
    bool hasValidGraph() const { return mpCurrentValidGraph != nullptr; }// Editor state queries
    bool isDirty() const { return mIsDirty; }

    // Pass management for editor
    void addPass(const std::string& name, ref<RenderPass> pass);
    void removePass(const std::string& name);
    void clearPasses();

    // Connection management for editor  
    void addConnection(const std::string& fromPass, const std::string& fromOutput, 
                      const std::string& toPass, const std::string& toInput);
    void removeConnection(const std::string& fromPass, const std::string& fromOutput, 
                         const std::string& toPass, const std::string& toInput);

    // Scene management
    void setScene(ref<Scene> scene);

private:
    void markDirty() { mIsDirty = true; }

    // Node editor implementation
    void initializeNodeIds();
    void setupNodeEditorStyle();
    void drawNodes();
    void drawConnections();
    void handleNodeEditorInput();
    
    // Helper functions
    bool findConnectionDetails(ed::PinId outputPinId, ed::PinId inputPinId, 
                             std::string& fromPass, std::string& fromOutput, 
                             std::string& toPass, std::string& toInput);
    int findOutputPinId(const std::string& passName, const std::string& outputName);
    int findInputPinId(const std::string& passName, const std::string& inputName);
    ImVec2 calculateNodePosition(const std::string& nodeName, size_t nodeIndex);
    bool removeConnectionByLinkId(int linkId);
    void autoLayoutNodes();
      // Editor state
    std::vector<RenderGraphNode> mEditorNodes;
    std::vector<RenderGraphConnection> mEditorConnections;
    ref<RenderGraph> mpCurrentValidGraph;
    ref<Scene> mpScene;
    bool mIsDirty;

    // Node editor context and styling
    ed::EditorContext* mpEditorContext;
    bool mStyleConfigured;
    
    // Node editor UI state
    std::unordered_map<std::string, ImVec2> mNodePositions;
    std::unordered_map<std::string, int> mNodeIds;
    std::unordered_map<std::string, std::vector<int>> mInputPinIds;
    std::unordered_map<std::string, std::vector<int>> mOutputPinIds;
    std::unordered_map<std::string, int> mLinkIds;
    int mNextId;
};
