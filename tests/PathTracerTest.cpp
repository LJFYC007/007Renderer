#include <gtest/gtest.h>

#include "Scene/Importer/AssimpImporter.h"
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
    const uint width = 1920;
    const uint height = 1080;
    AssimpImporter importer(mpDevice);
    ref<Scene> scene = importer.loadScene(std::string(PROJECT_DIR) + "/media/cornell_box.gltf");
    if (!scene)
        FAIL() << "Failed to load scene from file.";
    scene->buildAccelStructs();
    scene->camera = make_ref<Camera>(width, height, float3(0.f, 0.f, -5.f), float3(0.f, 0.f, -6.f), glm::radians(45.0f));
    scene->camera->calculateCameraParameters();

    // Create render graph
    auto renderGraph = RenderGraphBuilder::createDefaultGraph(mpDevice);
    renderGraph->setScene(scene);
    renderGraph->build();
    renderGraph->setScene(scene);
    RenderData finalOutput = renderGraph->execute();
    nvrhi::TextureHandle imageTexture = nvrhi::TextureHandle(static_cast<nvrhi::ITexture*>(finalOutput["ErrorMeasure.output"].Get()));
    ExrUtils::saveTextureToExr(mpDevice, imageTexture, "output.exr");
}