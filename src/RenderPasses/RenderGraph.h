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
    std::unordered_set<uint> dependencies; // Node indices this node depends on

    RenderGraphNode(const std::string& nodeName, ref<RenderPass> renderPass) : name(nodeName), pass(renderPass) {}
};

class RenderGraph
{
public:
    RenderGraph(ref<Device> pDevice);
    ~RenderGraph() = default;

    // Core graph building from external data
    static ref<RenderGraph> create(ref<Device> pDevice, 
                                   const std::vector<RenderGraphNode>& nodes,
                                   const std::vector<RenderGraphConnection>& connections);

    // Execute the entire render graph
    RenderData execute();   
    
    // Set scene for all passes that need it
    void setScene(ref<Scene> pScene);    

    // Graph queries
    const std::vector<RenderGraphNode>& getNodes() const { return mNodes; }
    const std::vector<RenderGraphConnection>& getConnections() const { return mConnections; }
    const std::vector<uint>& getExecutionOrder() const { return mExecutionOrder; }
    bool isBuilt() const { return mIsBuilt; }

private:
    // Core graph operations
    bool build(const std::vector<RenderGraphNode>& nodes, 
               const std::vector<RenderGraphConnection>& connections);
    bool topologicalSort();
    bool validateGraph();
    
    RenderData executePass(int nodeIndex);
    int findNode(const std::string& name) const;

    ref<Device> mpDevice;
    ref<Scene> mpScene;

    std::vector<RenderGraphNode> mNodes;
    std::vector<RenderGraphConnection> mConnections;
    std::vector<uint> mExecutionOrder;
    std::unordered_map<std::string, RenderData> mIntermediateResults;
    bool mIsBuilt;
};
