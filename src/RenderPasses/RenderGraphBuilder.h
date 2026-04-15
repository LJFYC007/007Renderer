#pragma once
#include "RenderGraph.h"
#include "PathTracingPass/PathTracing.h"
#include "AccumulatePass/Accumulate.h"
#include "ToneMappingPass/ToneMapping.h"
#include "ErrorMeasurePass/ErrorMeasure.h"
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
        nodes.emplace_back("ToneMapping", make_ref<ToneMappingPass>(pDevice));
        nodes.emplace_back("ErrorMeasure", make_ref<ErrorMeasurePass>(pDevice));
        nodes.emplace_back("TextureAverage", make_ref<TextureAverage>(pDevice));

        // Create connections
        std::vector<RenderGraphConnection> connections;
        connections.emplace_back("PathTracing", "output", "Accumulate", "input");
        connections.emplace_back("Accumulate", "output", "ToneMapping", "input");
        connections.emplace_back("ToneMapping", "output", "ErrorMeasure", "source");
        connections.emplace_back("ErrorMeasure", "output", "TextureAverage", "input");

        return RenderGraph::create(pDevice, nodes, connections);
    }
};
