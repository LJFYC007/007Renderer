#include <iostream>
#include <vector>
#include <spdlog/fmt/fmt.h>

#include "Core/Device.h"
#include "Core/Window.h"
#include "Scene/Importer/AssimpImporter.h"
#include "RenderPasses/RenderGraphBuilder.h"
#include "Utils/Math/Math.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"
#include "Utils/ExrUtils.h"
#include "Scene/Camera/Camera.h"

int main()
{
    Logger::init();
    auto logger = Logger::get();

    // -------------------------
    // 1. Initialize Device (D3D12 + NVRHI)
    // -------------------------
    ref<Device> device = make_ref<Device>();
    if (!device->initialize())
    {
        LOG_ERROR("Failed to initialize device!");
        return 1;
    }

    uint32_t width = 1920;
    uint32_t height = 1080;
    // Create window with configuration
    Window::desc windowDesc;
    windowDesc.width = width;
    windowDesc.height = height;
    windowDesc.title = "007Renderer";
    windowDesc.enableVSync = false;

    Window window(device->getD3D12Device(), device->getCommandQueue(), windowDesc);
    window.PrepareResources();

    {
        // -------------------------
        // 4. Setup triangle geometry for ray tracing
        // -------------------------
        AssimpImporter importer(device);
        ref<Scene> scene = importer.loadScene(std::string(PROJECT_DIR) + "/media/cornell_box.gltf");
        if (!scene)
        {
            LOG_ERROR("Failed to load scene from file.");
            return 1;
        }
        scene->buildAccelStructs();
        scene->camera = make_ref<Camera>(width, height, float3(0.f, 0.f, -5.f), float3(0.f, 0.f, -6.f), glm::radians(45.0f));

        // Create render graph
        auto renderGraph = RenderGraphBuilder::createDefaultGraph(device);
        renderGraph->setScene(scene);
        renderGraph->build();

        // -------------------------
        // 5. Setup GUI with original ImGui
        // -------------------------
        bool notDone = true;

        while (notDone)
        {
            HRESULT deviceRemovedReason = device->getD3D12Device()->GetDeviceRemovedReason();
            if (FAILED(deviceRemovedReason))
            {
                LOG_ERROR("Device removed: 0x{:08X}", static_cast<uint32_t>(deviceRemovedReason));

                // If in RDP environment, log additional info
                if (deviceRemovedReason == DXGI_ERROR_DEVICE_REMOVED)
                {
                    LOG_ERROR("This error commonly occurs in RDP environments. Consider using software rendering.");
                    LOG_ERROR("The application will now exit. Try running locally or use a different remote desktop solution.");
                }

                notDone = false;
                break;
            }

            device->getDevice()->runGarbageCollection();

            uint2 windowSize = window.GetWindowSize();
            if (windowSize.x != width || windowSize.y != height)
            {
                width = windowSize.x;
                height = windowSize.y;
                scene->camera->setWidth(width);
                scene->camera->setHeight(height);
                renderGraph->setScene(scene);
                LOG_DEBUG("Resized texture to {}x{}", width, height);
            }

            if (scene->camera->dirty)
                renderGraph->setScene(scene);
            scene->camera->calculateCameraParameters();

            // Execute render graph
            RenderData finalOutput = renderGraph->execute();

            // Set texture for display
            nvrhi::TextureHandle imageTexture = dynamic_cast<nvrhi::ITexture*>(finalOutput["ErrorMeasure.output"].Get());
            ID3D12Resource* d3d12Texture = static_cast<ID3D12Resource*>(imageTexture->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource));
            window.SetDisplayTexture(d3d12Texture);

            // Custom ImGui content before window render
            bool renderResult = window.RenderBegin();
            if (!renderResult)
            {
                notDone = false;
                break;
            }

            ImGuiIO& io = GUI::GetIO();
            GUI::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            GUI::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.3f, io.DisplaySize.y * 0.5f), ImGuiCond_Once);

            GUI::Begin("Settings");
            if (GUI::Button("Save image"))
                ExrUtils::saveTextureToExr(device, imageTexture, std::string(PROJECT_DIR) + "/output.exr");
            GUI::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / GUI::GetIO().Framerate, GUI::GetIO().Framerate);

            if (GUI::CollapsingHeader("Camera Controls"))
            {
                scene->camera->renderUI();
                scene->camera->handleInput();
            }
            renderGraph->renderUI();
            GUI::End();

            if (GUI::IsKeyPressed(ImGuiKey_Escape))
                notDone = false; // Exit on Escape key

            // Finish rendering
            window.RenderEnd();
        }
    }

    window.CleanupResources();
    device->shutdown();
    LOG_INFO("Renderer shutdown successfully.");
    spdlog::shutdown();
    return 0;
}