#pragma once
#include "RenderGraph.h"
#include "PathTracingPass/PathTracingPass.h"
#include "AccumulatePass/AccumulatePass.h"
#include "ErrorMeasure/ErrorMeasure.h"

class RenderGraphBuilder
{
public:
    static ref<RenderGraph> createDefaultGraph(ref<Device> device)
    {
        auto graph = make_ref<RenderGraph>(device);

        // Add passes
        graph->addPass("PathTracing", make_ref<PathTracingPass>(device));
        graph->addPass("Accumulate", make_ref<AccumulatePass>(device));
        graph->addPass("ErrorMeasure", make_ref<ErrorMeasure>(device));

        // Add connections following current logic:
        graph->addConnection("PathTracing", "output", "Accumulate", "output");
        graph->addConnection("Accumulate", "output", "ErrorMeasure", "output");

        return graph;
    }
};
