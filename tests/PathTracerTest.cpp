#include <gtest/gtest.h>
#include <algorithm>
#include <iostream>

#include "Scene/Importer/AssimpImporter.h"
#include "RenderPasses/RenderGraphEditor.h"
#include "RenderPasses/RenderGraphBuilder.h"
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
    const uint spp = 4096;
    const float threshold = 0.003f; // Convergence threshold

    AssimpImporter importer(mpDevice);
    ref<Scene> scene = importer.loadScene(std::string(PROJECT_DIR) + "/media/cornell_box.gltf");
    if (!scene)
        FAIL() << "Failed to load scene from file.";
    scene->buildAccelStructs();
    scene->camera = make_ref<Camera>(float3(0.f, 0.f, -5.f), float3(0.f, 0.f, -6.f), glm::radians(45.0f));

    // Create render graph
    RenderGraphEditor renderGraphEditor(mpDevice);
    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);

    // Render the scene
    for (uint i = 0; i < spp; ++i)
    {
        scene->camera->calculateCameraParameters();
        renderGraph->execute();
    }

    // Get the convergence result
    const auto& nodes = renderGraph->getNodes();
    ref<TextureAverage> textureAveragePass;
    for (const auto& node : nodes)
        if (node.name == "TextureAverage")
        {
            textureAveragePass = std::dynamic_pointer_cast<TextureAverage>(node.pass);
            break;
        }
    auto average = textureAveragePass->mAverageResult;
    EXPECT_TRUE(average.r < threshold && average.g < threshold && average.b < threshold);

    // Save the final output texture to EXR file
    nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
    ExrUtils::saveTextureToExr(mpDevice, imageTexture, "output.exr");
}
