#pragma once
#include "RenderGraph.h"
#include "PathTracingPass/PathTracingPass.h"
#include "AccumulatePass/AccumulatePass.h"
#include "ErrorMeasure/ErrorMeasure.h"

class RenderGraphBuilder
{
public:
    static ref<RenderGraph> createDefaultGraph(ref<Device> pDevice)
    {
        auto pGraph = make_ref<RenderGraph>(pDevice);

        // Add passes
        pGraph->addPass("PathTracing", make_ref<PathTracingPass>(pDevice));
        pGraph->addPass("Accumulate", make_ref<AccumulatePass>(pDevice));
        pGraph->addPass("ErrorMeasure", make_ref<ErrorMeasure>(pDevice));

        // Add connections following current logic:
        pGraph->addConnection("PathTracing", "output", "Accumulate", "output");
        pGraph->addConnection("Accumulate", "output", "ErrorMeasure", "output");

        return pGraph;
    }
};
