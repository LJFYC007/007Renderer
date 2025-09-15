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
    std::unordered_set<std::string> dependencies; // Passes this node depends on

    RenderGraphNode(const std::string& nodeName, ref<RenderPass> renderPass) : name(nodeName), pass(renderPass) {}
};

class RenderGraph
{
public:
    RenderGraph(ref<Device> device) : mpDevice(device) {}

    // Add passes and connections
    void addPass(const std::string& name, ref<RenderPass> pass);
    void addConnection(const std::string& fromPass, const std::string& fromOutput, const std::string& toPass, const std::string& toInput);

    // Build and validate the graph
    bool build();

    // Execute the entire render graph
    RenderData execute();

    // Set scene for all passes that need it
    void setScene(ref<Scene> scene);

    // Render UI for all passes
    void renderUI();

private:
    ref<Device> mpDevice;
    ref<Scene> mpScene;

    std::vector<RenderGraphNode> mNodes;
    std::vector<RenderGraphConnection> mConnections;
    std::vector<std::string> mExecutionOrder; // Topologically sorted pass names

    bool topologicalSort();

    // Execution helpers
    RenderData executePass(int nodeIndex, const std::unordered_map<std::string, RenderData>& intermediateResults);
    int findNode(const std::string& name);
};
