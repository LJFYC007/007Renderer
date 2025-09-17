#include <iostream>
#include <vector>
#include <spdlog/fmt/fmt.h>

#include "Core/Device.h"
#include "Core/Window.h"
#include "Scene/Importer/AssimpImporter.h"
#include "RenderPasses/RenderGraphBuilder.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"
#include "Scene/Camera/Camera.h"

int main()
{
    Logger::init();
    auto logger = Logger::get();

    // Initialize device (D3D12 + NVRHI)
    ref<Device> pDevice = make_ref<Device>();
    if (!pDevice->initialize())
    {
        LOG_ERROR("Failed to initialize pDevice!");
        return 1;
    }

    // Create imgui window with configuration
    Window::desc windowDesc;
    windowDesc.width = 2424;
    windowDesc.height = 1519;
    windowDesc.title = "007Renderer";
    windowDesc.enableVSync = false;

    Window window(pDevice->getD3D12Device(), pDevice->getCommandQueue(), windowDesc);
    window.PrepareResources();

    {
        // Setup scene
        AssimpImporter importer(pDevice);
        ref<Scene> scene = importer.loadScene(std::string(PROJECT_DIR) + "/media/cornell_box.gltf");
        if (!scene)
        {
            LOG_ERROR("Failed to load scene from file.");
            return 1;
        }
        scene->buildAccelStructs();
        uint width = 1920, height = 1080;
        scene->camera = make_ref<Camera>(width, height, float3(0.f, 0.f, -5.f), float3(0.f, 0.f, -6.f), glm::radians(45.0f));

        // Create render graph
        auto renderGraph = RenderGraphBuilder::createDefaultGraph(pDevice);
        renderGraph->setScene(scene);
        renderGraph->build();

        GUIManager guiManager(pDevice);
        bool notDone = true;
        while (notDone)
        {
            HRESULT pDeviceRemovedReason = pDevice->getD3D12Device()->GetDeviceRemovedReason();
            if (FAILED(pDeviceRemovedReason))
            {
                LOG_ERROR("Device removed: 0x{:08X}", static_cast<uint32_t>(pDeviceRemovedReason));
                // If in RDP environment, log additional info
                if (pDeviceRemovedReason == DXGI_ERROR_DEVICE_REMOVED)
                {
                    LOG_ERROR("This error commonly occurs in RDP environments. Consider using software rendering.");
                    LOG_ERROR("The application will now exit. Try running locally or use a different remote desktop solution.");
                }
                notDone = false;
                break;
            }
            pDevice->getDevice()->runGarbageCollection();

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

            guiManager.renderMainLayout(scene, renderGraph, imageTexture, window, width, height);
            if (GUI::IsKeyPressed(ImGuiKey_Escape))
                notDone = false; // Exit on Escape key

            // Finish rendering
            window.RenderEnd();
        }
    }

    window.CleanupResources();
    pDevice->shutdown();
    LOG_INFO("Renderer shutdown successfully.");
    spdlog::shutdown();
    return 0;
}