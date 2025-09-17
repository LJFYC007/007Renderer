#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <imgui.h>
#include <imgui_node_editor.h>

#include "RenderPass.h"
#include "Core/Pointer.h"

namespace ed = ax::NodeEditor;

struct RenderGraphConnection
{
    std::string fromPass;
    std::string fromOutput;
    std::string toPass;
    std::string toInput;

    RenderGraphConnection(const std::string& from, const std::string& fromOut, const std::string& to, const std::string& toIn)
        : fromPass(from), fromOutput(fromOut), toPass(to), toInput(toIn)
    {}
};

struct RenderGraphNode
{
    std::string name;
    ref<RenderPass> pass;
    std::unordered_set<uint> dependencies; // Node indices this node depends on

    RenderGraphNode(const std::string& nodeName, ref<RenderPass> renderPass) : name(nodeName), pass(renderPass) {}
};

class RenderGraph
{
public:
    RenderGraph(ref<Device> device);
    ~RenderGraph();

    // Add passes and connections
    void addPass(const std::string& name, ref<RenderPass> pass);
    void addConnection(const std::string& fromPass, const std::string& fromOutput, const std::string& toPass, const std::string& toInput);

    // Build and validate the graph
    void build();

    // Execute the entire render graph
    RenderData execute();

    // Set scene for all passes that need it
    void setScene(ref<Scene> scene);    
    
    // Render UI for all passes
    void renderUI();

    // Render node editor visualization
    void renderNodeEditor();

private:
    bool topologicalSort();

    RenderData executePass(int nodeIndex);

    int findNode(const std::string& name);    
    
    // Node editor helpers
    void initializeNodeIds();
    void drawNodes();
    void drawConnections();
    void handleNodeEditorInput();
    bool findConnectionDetails(ed::PinId outputPinId, ed::PinId inputPinId, 
                             std::string& fromPass, std::string& fromOutput, 
                             std::string& toPass, std::string& toInput);    int findOutputPinId(const std::string& passName, const std::string& outputName);    int findInputPinId(const std::string& passName, const std::string& inputName);
    ImVec2 calculateNodePosition(const std::string& nodeName, size_t nodeIndex);
    void removeConnectionByLinkId(int linkId);
    void autoLayoutNodes();

    ref<Device> mpDevice;
    ref<Scene> mpScene;

    std::vector<RenderGraphNode> mNodes;
    std::vector<RenderGraphConnection> mConnections;
    std::vector<uint> mExecutionOrder; // Topologically sorted node indices
    std::unordered_map<std::string, RenderData> intermediateResults;

    // Node editor context
    ed::EditorContext* mpEditorContext;
      // Node positions and IDs for editor
    std::unordered_map<std::string, ImVec2> mNodePositions;
    std::unordered_map<std::string, int> mNodeIds;
    std::unordered_map<std::string, std::vector<int>> mInputPinIds;  // Pass name -> input pin IDs
    std::unordered_map<std::string, std::vector<int>> mOutputPinIds; // Pass name -> output pin IDs
    std::unordered_map<std::string, int> mLinkIds;                   // Connection key -> link ID
    int mNextId;
};
