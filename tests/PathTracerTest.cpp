#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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

namespace
{
// Mean absolute per-channel error vs reference.exr at 4096 spp.
// Measured 2026-04-21: r=7.98e-4 g=6.22e-4 b=3.22e-4. The r-channel passes by only
// ~0.3% margin, so minor sampler-side variance may flake. If intermittent failures
// appear, re-baseline reference.exr or loosen to ~1.5e-3 — but document why.
constexpr float kCornellThreshold = 0.0008f;

// Mean absolute per-channel deviation from 1.0 after 1024 spp in the weak white furnace.
// Calibration (2026-04-21, 1024 spp): error grows with roughness —
//   r=0.05 → 4.7e-5,  r=0.25 → 3.8e-4,  r=0.5 → 1.2e-3,  r=0.75 → 2.2e-3,  r=1.0 → 3.0e-3.
// Threshold 0.03 gives ~10× headroom so variance can wobble but any energy-conservation
// regression >1% still trips the test.
constexpr float kFurnaceThreshold = 0.03f;
} // namespace

class PathTracer : public DeviceTest
{};

TEST_F(PathTracer, Smoke)
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

TEST_F(PathTracer, CornellConverges)
{
    if (TestHelpers::isFastMode())
        GTEST_SKIP() << "slow convergence test; unset RENDERER_FAST_TESTS to run";

    const uint spp = 4096;

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
    std::cout << "PathTracer.CornellConverges avg error vs reference: r=" << average.r << " g=" << average.g << " b=" << average.b << std::endl;
    EXPECT_TRUE(average.r < kCornellThreshold && average.g < kCornellThreshold && average.b < kCornellThreshold)
        << "Average result: r=" << average.r << ", g=" << average.g << ", b=" << average.b;

    // Dump the render only when the test failed — passing runs stay hermetic.
    if (::testing::Test::HasFailure())
    {
        nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
        ExrUtils::saveTextureToExr(mpDevice, imageTexture, TestHelpers::artifactPath("output.exr"));
    }
}

// Convergence curve for path tracing — no PASS/FAIL. Captures {spp, relMSE} rows for
// later comparison against a Light-BVH implementation. To rebaseline, copy the artifact
// CSV over tests/benchmarks/bistro_baseline.csv.
class PathTracerBench : public BenchmarkTest
{};

TEST_F(PathTracerBench, BistroCurve)
{
    const char* envScenePath = std::getenv("RENDERER_BISTRO_PATH");
    const std::string scenePath = envScenePath ? envScenePath : "D:/Scenes/Bistro_v5_2/BistroInterior_Wine.usdc";
    const std::string refPath = std::string(PROJECT_DIR) + "/media/bistro_reference.exr";
    if (!std::filesystem::exists(scenePath) || !std::filesystem::exists(refPath))
        GTEST_SKIP() << "Bistro scene or bistro_reference.exr not available locally.";

    const std::vector<uint> checkpoints = {8, 16, 32, 64, 128, 256, 512, 1024, 2048};

    std::map<uint, float> baseline;
    const std::string baselinePath = std::string(PROJECT_DIR) + "/tests/benchmarks/bistro_baseline.csv";
    if (std::ifstream ifs(baselinePath); ifs)
    {
        std::string line;
        std::getline(ifs, line); // header
        while (std::getline(ifs, line))
        {
            uint spp;
            float err;
            if (std::sscanf(line.c_str(), "%u,%f", &spp, &err) == 2)
                baseline.emplace(spp, err);
        }
    }
    else
    {
        std::cout << "No baseline at " << baselinePath << " — copy this run's CSV there to establish one." << std::endl;
    }

    ref<Scene> scene = loadSceneWithImporter(scenePath, mpDevice);
    ASSERT_NE(scene, nullptr) << "Failed to load Bistro scene.";
    scene->buildAccelStructs();

    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);

    auto errPass = renderGraph->getPassByName<ErrorMeasurePass>("ErrorMeasure");
    auto avgPass = renderGraph->getPassByName<TextureAverage>("TextureAverage");
    ASSERT_NE(errPass, nullptr);
    ASSERT_NE(avgPass, nullptr);

    errPass->setTextureReference(refPath);
    errPass->setMetric(ErrorMeasurePass::ErrorMetric::RelMSE);

    std::ofstream csv(TestHelpers::artifactPath("bistro_convergence.csv"));
    csv << "spp,relMSE\n";

    std::cout << "\nBistro convergence (relMSE vs reference):\n"
              << "   spp      relMSE    baseline     delta\n";

    uint rendered = 0;
    for (uint target : checkpoints)
    {
        while (rendered < target)
        {
            scene->camera->calculateCameraParameters();
            renderGraph->execute();
            ++rendered;
        }
        auto e = avgPass->getAverageResult();
        const float err = (e.r + e.g + e.b) / 3.f;

        std::cout << std::setw(6) << target << "  " << std::fixed << std::setprecision(6) << std::setw(10) << err;
        if (auto it = baseline.find(target); it != baseline.end())
        {
            const float rel = (err - it->second) / it->second * 100.f;
            std::cout << "  " << std::setw(10) << it->second << "  " << std::showpos << std::setprecision(3) << std::setw(7) << rel << std::noshowpos
                      << " %";
        }
        std::cout << std::defaultfloat << std::endl;
        csv << target << "," << err << "\n";
    }
}

// Weak white furnace test (Heitz 2014 Sec 5.2)
//
// Sets metallic=1, baseColor=white (F=1 everywhere), uses G1-only masking, and checks
// convergence to 1.0. Single-bounce integral: BRDF sample weight accumulated against
// a constant white environment, then terminated. Below-surface reflections (wo.z < 0)
// are kept because G1 normalization integrates over all microfacet normals — the sample
// weight G1(wi)*dot(wo,h)/(wi.z*h.z) is independent of sign(wo.z). Production-path
// isValidScatter() rejection is intentionally bypassed in furnace mode.
//
// Validates the *weak* furnace only. The production BRDF with G2 joint masking still
// shows single-scattering energy loss at high roughness — that needs Kulla-Conty (2017)
// multi-scattering compensation, which is a separate task.
class WhiteFurnace : public DeviceTest, public ::testing::WithParamInterface<float>
{};

TEST_P(WhiteFurnace, Converges)
{
    if (TestHelpers::isFastMode())
        GTEST_SKIP() << "slow convergence test; unset RENDERER_FAST_TESTS to run";

    const float roughness = GetParam();
    const uint spp = 1024;

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

    std::cout << "WhiteFurnace roughness=" << roughness << " avg error: r=" << avg.r << " g=" << avg.g << " b=" << avg.b << std::endl;

    EXPECT_LT(avg.r, kFurnaceThreshold);
    EXPECT_LT(avg.g, kFurnaceThreshold);
    EXPECT_LT(avg.b, kFurnaceThreshold);

    if (::testing::Test::HasFailure())
    {
        nvrhi::TextureHandle outputTex = renderGraph->getFinalOutputTexture();
        if (outputTex)
            ExrUtils::saveTextureToExr(mpDevice, outputTex, TestHelpers::artifactPath("furnace_diff.exr"));
    }
}

INSTANTIATE_TEST_SUITE_P(
    RoughnessSweep,
    WhiteFurnace,
    ::testing::Values(0.05f, 0.25f, 0.5f, 0.75f, 1.0f),
    [](const ::testing::TestParamInfo<float>& info)
    {
        // TestParamInfo's test name must be a valid C identifier — dots in "0.05" would break it.
        int scaled = static_cast<int>(info.param * 100.0f + 0.5f);
        return "r" + std::to_string(scaled);
    }
);
