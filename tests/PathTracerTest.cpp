#include <gtest/gtest.h>
#include <algorithm>
#include <iostream>

#include "Scene/Importer/Importer.h"
#include "RenderPasses/RenderGraphBuilder.h"
#include "RenderPasses/PathTracingPass/PathTracing.h"
#include "RenderPasses/AccumulatePass/Accumulate.h"
#include "RenderPasses/ErrorMeasurePass/ErrorMeasure.h"
#include "Utils/ExrUtils.h"
#include "Environment.h"

class PathTracerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mpDevice = BasicTestEnvironment::getDevice();
        ASSERT_NE(mpDevice, nullptr);
        ASSERT_TRUE(mpDevice->isValid());
    }

    ref<Device> mpDevice;
};

TEST_F(PathTracerTest, Basic)
{
    const uint spp = 4;           // Low SPP for CI
    const float threshold = 1.0f; // Large threshold

    ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/cornell_box.usdc", mpDevice);
    if (!scene)
        FAIL() << "Failed to load scene from file.";
    scene->buildAccelStructs();

    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);

    for (uint i = 0; i < spp; ++i)
    {
        scene->camera->calculateCameraParameters();
        renderGraph->execute();
    }

    // Only verify that rendering completed without errors
    nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
    EXPECT_NE(imageTexture, nullptr);
}

TEST_F(PathTracerTest, Full)
{
    const uint spp = 4096;
    const float threshold = 0.0008f; // Convergence threshold

    // ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/cornell_box.gltf", mpDevice);
    ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/cornell_box.usdc", mpDevice);
    if (!scene)
        FAIL() << "Failed to load scene from file.";
    scene->buildAccelStructs();

    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);

    auto errorMeasure = renderGraph->getPassByName<ErrorMeasure>("ErrorMeasure");
    ASSERT_NE(errorMeasure, nullptr);
    errorMeasure->setTextureReference(std::string(PROJECT_DIR) + "/media/reference.exr");

    for (uint i = 0; i < spp; ++i)
    {
        scene->camera->calculateCameraParameters();
        renderGraph->execute();
    }

    auto textureAveragePass = renderGraph->getPassByName<TextureAverage>("TextureAverage");
    auto average = textureAveragePass->getAverageResult();
    EXPECT_TRUE(average.r < threshold && average.g < threshold && average.b < threshold)
        << "Average result: r=" << average.r << ", g=" << average.g << ", b=" << average.b;

    // Save the final output texture to EXR file
    nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
    ExrUtils::saveTextureToExr(mpDevice, imageTexture, "output.exr");
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
TEST_F(PathTracerTest, WhiteFurnaceFull)
{
    const uint spp = 1024;
    const float threshold = 0.03f;
    const float roughnessValues[] = {0.05f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float roughness : roughnessValues)
    {
        // Load scene fresh for each roughness
        ref<Scene> scene = loadSceneWithImporter(std::string(PROJECT_DIR) + "/media/sphere.usdc", mpDevice);
        ASSERT_NE(scene, nullptr) << "Failed to load scene";

        // Set roughness on all materials (furnace mode overrides baseColor/metallic/etc on the GPU)
        for (auto& mat : scene->materials)
            mat.roughnessFactor = roughness;
        scene->buildAccelStructs();

        // Build a custom graph that bypasses ToneMapping — the weak furnace test
        // validates BRDF energy conservation in linear radiance space, and the gamma
        // curve in ToneMappingPass would distort the error magnitudes the threshold
        // is calibrated against.
        std::vector<RenderGraphNode> nodes;
        nodes.emplace_back("PathTracing", make_ref<PathTracingPass>(mpDevice));
        nodes.emplace_back("Accumulate", make_ref<AccumulatePass>(mpDevice));
        nodes.emplace_back("ErrorMeasure", make_ref<ErrorMeasure>(mpDevice));
        nodes.emplace_back("TextureAverage", make_ref<TextureAverage>(mpDevice));
        std::vector<RenderGraphConnection> connections;
        connections.emplace_back("PathTracing", "output", "Accumulate", "input");
        connections.emplace_back("Accumulate", "output", "ErrorMeasure", "source");
        connections.emplace_back("ErrorMeasure", "output", "TextureAverage", "input");
        auto renderGraph = RenderGraph::create(mpDevice, nodes, connections);

        auto pathTracing = renderGraph->getPassByName<PathTracingPass>("PathTracing");
        ASSERT_NE(pathTracing, nullptr);
        pathTracing->setMissColor(1.0f);
        pathTracing->setFurnaceMode(FurnaceMode::WeakWhiteFurnace);

        auto errorMeasure = renderGraph->getPassByName<ErrorMeasure>("ErrorMeasure");
        ASSERT_NE(errorMeasure, nullptr);
        errorMeasure->setConstantReference({1.f, 1.f, 1.f});

        renderGraph->setScene(scene);

        // Render
        for (uint i = 0; i < spp; ++i)
        {
            scene->camera->calculateCameraParameters();
            renderGraph->execute();
        }

        auto textureAverage = renderGraph->getPassByName<TextureAverage>("TextureAverage");
        ASSERT_NE(textureAverage, nullptr);
        auto avg = textureAverage->getAverageResult();

        std::cout << "WeakWhiteFurnace roughness=" << roughness << " avg error: r=" << avg.r << " g=" << avg.g << " b=" << avg.b << std::endl;

        // Save debug EXRs
        std::string suffix = std::to_string(roughness);
        nvrhi::TextureHandle outputTex = renderGraph->getFinalOutputTexture();
        if (outputTex)
            ExrUtils::saveTextureToExr(mpDevice, outputTex, "furnace_diff_rough" + suffix + ".exr");

        // Assert convergence to white
        EXPECT_LT(avg.r, threshold) << "roughness=" << roughness;
        EXPECT_LT(avg.g, threshold) << "roughness=" << roughness;
        EXPECT_LT(avg.b, threshold) << "roughness=" << roughness;
    }
}
