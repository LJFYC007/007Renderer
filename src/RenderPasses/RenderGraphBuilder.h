#pragma once
#include "RenderGraph.h"
#include "PathTracingPass/PathTracingPass.h"
#include "AccumulatePass/AccumulatePass.h"
#include "ErrorMeasure/ErrorMeasure.h"
#include "Utils/TextureAverage/TextureAverage.h"

class RenderGraphBuilder
{
public:
    static ref<RenderGraph> createDefaultGraph(ref<Device> pDevice)
    {
        // Create nodes
        std::vector<RenderGraphNode> nodes;
        nodes.emplace_back("PathTracing", make_ref<PathTracingPass>(pDevice));
        nodes.emplace_back("Accumulate", make_ref<AccumulatePass>(pDevice));
        nodes.emplace_back("ErrorMeasure", make_ref<ErrorMeasure>(pDevice));
        nodes.emplace_back("TextureAverage", make_ref<TextureAverage>(pDevice));

        // Create connections
        std::vector<RenderGraphConnection> connections;
        connections.emplace_back("PathTracing", "output", "Accumulate", "input");
        connections.emplace_back("Accumulate", "output", "ErrorMeasure", "source");
        connections.emplace_back("ErrorMeasure", "output", "TextureAverage", "input");

        return RenderGraph::create(pDevice, nodes, connections);
    }
};
