#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "Scene/Importer/Importer.h"
#include "RenderPasses/RenderGraphBuilder.h"
#include "RenderPasses/PathTracingPass/PathTracing.h"
#include "RenderPasses/AccumulatePass/Accumulate.h"
#include "RenderPasses/ErrorMeasurePass/ErrorMeasure.h"
#include "Utils/ExrUtils.h"
#include "Utils/ResourceIO.h"
#include "Environment.h"
#include "TestHelpers.h"

class PathTracerTest : public DeviceTest
{};

TEST_F(PathTracerTest, Basic)
{
    const uint spp = 4;

    ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/cornell_box.usdc", mpDevice);
    ASSERT_NE(scene, nullptr) << "Failed to load scene from file.";
    scene->buildAccelStructs();

    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);

    for (uint i = 0; i < spp; ++i)
    {
        // Camera jitter advances per frame (Camera.cpp:99); hoisting collapses spp to 1.
        scene->camera->calculateCameraParameters();
        renderGraph->execute();
    }

    nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
    ASSERT_NE(imageTexture, nullptr);
}

TEST_F(PathTracerTest, Full)
{
    const uint spp = 4096;

    // Convergence threshold = mean absolute per-channel error vs reference.exr at 4096 spp.
    // WARNING: threshold is *tight*. Measured on 2026-04-21: r=7.98e-4 g=6.22e-4 b=3.22e-4.
    // The r-channel passes by only ~0.3% margin, so minor sampler-side variance may flake
    // this test. Consider re-baselining the reference.exr or loosening threshold to ~1.5e-3
    // if intermittent failures show up. Do not silently loosen it — document why the new
    // threshold is safe.
    const float threshold = 0.0008f;

    ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/cornell_box.usdc", mpDevice);
    ASSERT_NE(scene, nullptr) << "Failed to load scene from file.";
    scene->buildAccelStructs();

    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);

    auto errorMeasure = renderGraph->getPassByName<ErrorMeasurePass>("ErrorMeasure");
    ASSERT_NE(errorMeasure, nullptr);
    errorMeasure->setTextureReference(std::string(PROJECT_DIR) + "/media/reference.exr");

    for (uint i = 0; i < spp; ++i)
    {
        scene->camera->calculateCameraParameters();
        renderGraph->execute();
    }

    auto textureAveragePass = renderGraph->getPassByName<TextureAverage>("TextureAverage");
    auto average = textureAveragePass->getAverageResult();
    std::cout << "PathTracerTest.Full avg error vs reference: r=" << average.r << " g=" << average.g << " b=" << average.b << std::endl;
    EXPECT_TRUE(average.r < threshold && average.g < threshold && average.b < threshold)
        << "Average result: r=" << average.r << ", g=" << average.g << ", b=" << average.b;

    // Dump the render only when the test failed — passing runs stay hermetic.
    if (::testing::Test::HasFailure())
    {
        nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
        ExrUtils::saveTextureToExr(mpDevice, imageTexture, TestHelpers::artifactPath("output.exr"));
    }
}

// Weak white furnace test (Heitz 2014 Sec 5.2)
//
// Sets metallic=1, baseColor=white (F=1 everywhere), uses G1-only masking, and checks
// convergence to 1.0. Evaluates a single-bounce integral: on each hit the BRDF sample weight
// is accumulated against the constant white environment and the path terminates immediately.
// Below-surface reflections (wo.z < 0) are kept because the G1 normalization identity
// integrates over all microfacet normals — the sample weight G1(wi)*dot(wo,h)/(wi.z*h.z)
// is independent of the sign of wo.z. Production-path isValidScatter() rejection is
// intentionally bypassed in furnace mode.
//
// Note: validates the *weak* furnace (single-scattering G1 normalization) only. The production
// BRDF with G2 joint masking still exhibits single-scattering energy loss at high roughness,
// which requires Kulla-Conty (2017) multi-scattering compensation — that is a separate task.
class WhiteFurnaceFull : public DeviceTest, public ::testing::WithParamInterface<float>
{};

TEST_P(WhiteFurnaceFull, ConvergesToWhite)
{
    const float roughness = GetParam();
    const uint spp = 1024;

    // Convergence threshold = mean absolute per-channel deviation from 1.0 after 1024 spp.
    // Calibration (2026-04-21, 1024 spp): observed avg error grows with roughness —
    //   r=0.05 → 4.7e-5,  r=0.25 → 3.8e-4,  r=0.5 → 1.2e-3,  r=0.75 → 2.2e-3,  r=1.0 → 3.0e-3.
    // Threshold 0.03 gives ~10× headroom over the worst (r=1.0) baseline, so variance can
    // wobble run-to-run but any energy-conservation regression >1% still trips the test.
    const float threshold = 0.03f;

    ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/sphere.usdc", mpDevice);
    ASSERT_NE(scene, nullptr) << "Failed to load scene";

    // Furnace mode overrides baseColor/metallic on the GPU; roughness is set here and read by shader.
    for (auto& mat : scene->materials)
        mat.roughnessFactor = roughness;
    scene->buildAccelStructs();

    // Custom graph bypasses ToneMapping — the weak furnace test validates BRDF energy
    // conservation in linear radiance space, and the gamma curve in ToneMappingPass would
    // distort the error magnitudes the threshold is calibrated against.
    std::vector<RenderGraphNode> nodes;
    nodes.emplace_back("PathTracing", make_ref<PathTracingPass>(mpDevice));
    nodes.emplace_back("Accumulate", make_ref<AccumulatePass>(mpDevice));
    nodes.emplace_back("ErrorMeasure", make_ref<ErrorMeasurePass>(mpDevice));
    nodes.emplace_back("TextureAverage", make_ref<TextureAverage>(mpDevice));
    std::vector<RenderGraphConnection> connections;
    connections.emplace_back("PathTracing", "output", "Accumulate", "input");
    connections.emplace_back("Accumulate", "output", "ErrorMeasure", "source");
    connections.emplace_back("ErrorMeasure", "output", "TextureAverage", "input");
    auto renderGraph = RenderGraph::create(mpDevice, nodes, connections);
    ASSERT_NE(renderGraph, nullptr);

    auto pathTracing = renderGraph->getPassByName<PathTracingPass>("PathTracing");
    ASSERT_NE(pathTracing, nullptr);
    pathTracing->setMissColor(1.0f);
    pathTracing->setFurnaceMode(FurnaceMode::WeakWhiteFurnace);

    auto errorMeasure = renderGraph->getPassByName<ErrorMeasurePass>("ErrorMeasure");
    ASSERT_NE(errorMeasure, nullptr);
    errorMeasure->setConstantReference({1.f, 1.f, 1.f});

    renderGraph->setScene(scene);

    for (uint i = 0; i < spp; ++i)
    {
        scene->camera->calculateCameraParameters();
        renderGraph->execute();
    }

    auto textureAverage = renderGraph->getPassByName<TextureAverage>("TextureAverage");
    ASSERT_NE(textureAverage, nullptr);
    auto avg = textureAverage->getAverageResult();

    std::cout << "WeakWhiteFurnace roughness=" << roughness << " avg error: r=" << avg.r << " g=" << avg.g << " b=" << avg.b << std::endl;

    EXPECT_LT(avg.r, threshold);
    EXPECT_LT(avg.g, threshold);
    EXPECT_LT(avg.b, threshold);

    if (::testing::Test::HasFailure())
    {
        nvrhi::TextureHandle outputTex = renderGraph->getFinalOutputTexture();
        if (outputTex)
            ExrUtils::saveTextureToExr(mpDevice, outputTex, TestHelpers::artifactPath("furnace_diff.exr"));
    }
}

INSTANTIATE_TEST_SUITE_P(
    RoughnessSweep,
    WhiteFurnaceFull,
    ::testing::Values(0.05f, 0.25f, 0.5f, 0.75f, 1.0f),
    [](const ::testing::TestParamInfo<float>& info)
    {
        // TestParamInfo's test name must be a valid C identifier — dots in "0.05" would break it.
        int scaled = static_cast<int>(info.param * 100.0f + 0.5f);
        return "r" + std::to_string(scaled);
    }
);
