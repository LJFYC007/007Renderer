#include <iostream>
#include <vector>
#include <spdlog/fmt/fmt.h>

#include "Core/Device.h"
#include "Core/Window.h"
#include "Scene/Importer/UsdImporter.h"
#include "Scene/Importer/AssimpImporter.h"
#include "RenderPasses/RenderGraphBuilder.h"
#include "RenderPasses/RenderGraphEditor.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"
#include "Utils/ResourceIO.h"
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
    windowDesc.height = 1719;
    windowDesc.title = "007Renderer";
    windowDesc.enableVSync = false;

    Window window(pDevice->getD3D12Device(), pDevice->getCommandQueue(), windowDesc);
    window.PrepareResources();

    {
        // Create readback heap
        gReadbackHeap = make_ref<ReadbackHeap>(pDevice);

        // Setup scene
        UsdImporter importer(pDevice);
        ref<Scene> scene = importer.loadScene(std::string(PROJECT_DIR) + "/media/cornell_box/cornell_box.usdc");
        if (!scene)
        {
            LOG_ERROR("Failed to load scene from file.");
            return 1;
        }
        scene->buildAccelStructs();

        // Create render graph editor and initialize with default graph
        RenderGraphEditor renderGraphEditor(pDevice);
        auto defaultRenderGraph = RenderGraphBuilder::createDefaultGraph(pDevice);
        defaultRenderGraph->setScene(scene);

        // Initialize editor from the default graph (this populates the editor's node/connection lists)
        renderGraphEditor.initializeFromRenderGraph(defaultRenderGraph);
        renderGraphEditor.setScene(scene);

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

            // Update camera parameters if dirty
            if (scene->camera->dirty)
                renderGraphEditor.setScene(scene);
            scene->camera->calculateCameraParameters(); // Remember this contains jitter

            // Get current render graph and execute
            auto renderGraph = renderGraphEditor.getCurrentRenderGraph();
            RenderData finalOutput;
            finalOutput = renderGraph->execute();

            // Set texture for display using the selected output
            nvrhi::TextureHandle imageTexture = renderGraph->getFinalOutputTexture();
            ID3D12Resource* d3d12Texture = static_cast<ID3D12Resource*>(imageTexture->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource));
            window.SetDisplayTexture(d3d12Texture);

            // Custom ImGui content before window render
            Window::FrameStatus frameStatus = window.RenderBegin();
            if (frameStatus == Window::FrameStatus::Exit)
            {
                notDone = false;
                break;
            }
            else if (frameStatus == Window::FrameStatus::Skip)
                continue;

            guiManager.renderMainLayout(scene, &renderGraphEditor, imageTexture, window);
            if (GUI::IsKeyPressed(ImGuiKey_Escape))
                notDone = false; // Exit on Escape key

            // Finish rendering
            window.RenderEnd();
        }

        // Release readback heap before device shutdown
        gReadbackHeap.reset();
    }

    window.CleanupResources();
    pDevice->shutdown();
    LOG_INFO("Renderer shutdown successfully.");
    spdlog::shutdown();
    return 0;
}