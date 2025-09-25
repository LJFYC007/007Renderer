#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "RenderPass.h"
#include "Core/Pointer.h"

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

    RenderGraphNode(const std::string& nodeName, ref<RenderPass> renderPass) : name(nodeName), pass(renderPass) {}
};

class RenderGraph
{
public:
    RenderGraph(ref<Device> pDevice) : mpDevice(pDevice), mSelectedOutputIndex(0) {}
    ~RenderGraph() = default;

    // Core graph building from external data
    static ref<RenderGraph> create(
        ref<Device> pDevice,
        const std::vector<RenderGraphNode>& nodes,
        const std::vector<RenderGraphConnection>& connections
    );

    // Execute the entire render graph
    RenderData execute();

    // Get the final output texture for display (based on UI selection)
    nvrhi::TextureHandle getFinalOutputTexture();

    // Render UI for output selection
    void renderOutputSelectionUI();

    // Set scene for all passes that need it
    void setScene(ref<Scene> pScene);

    // Graph queries
    const std::vector<RenderGraphNode>& getNodes() const { return mNodes; }
    const std::vector<RenderGraphConnection>& getConnections() const { return mConnections; }
    const std::vector<uint>& getExecutionOrder() const { return mExecutionOrder; }

private:
    // Core graph operations
    bool build(const std::vector<RenderGraphNode>& nodes, const std::vector<RenderGraphConnection>& connections);
    bool topologicalSort();
    bool validateGraph();
    void buildDependencyGraph();

    RenderData executePass(int nodeIndex);
    int findNode(const std::string& name) const;
    void createOutputTexture(uint32_t width, uint32_t height, nvrhi::Format format);

    ref<Device> mpDevice;
    ref<Scene> mpScene;

    std::vector<RenderGraphNode> mNodes;
    std::vector<RenderGraphConnection> mConnections;
    std::vector<std::unordered_set<uint>> mDependencies; // Node indices this node depends on
    std::vector<uint> mExecutionOrder;
    std::unordered_map<std::string, RenderData> mIntermediateResults; // UI state for output selection
    std::string mSelectedOutputKey;
    std::vector<std::string> mAvailableOutputs;
    int mSelectedOutputIndex;

    // Internal output texture for maintaining ShaderResource state
    nvrhi::TextureHandle mOutputTexture;
    uint32_t mOutputWidth = 0;
    uint32_t mOutputHeight = 0;
};
