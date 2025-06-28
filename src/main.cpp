#include <iostream>
#include <vector>
#include <spdlog/fmt/fmt.h>

#include "Core/Device.h"
#include "Core/Window.h"
#include "Core/Buffer.h"
#include "Core/Texture.h"
#include "RenderPasses/ComputePass.h"
#include "RenderPasses/RayTracingPass.h"
#include "Utils/Math/Math.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"
#include "Scene/Camera/Camera.h"

int main()
{
    Logger::init();
    auto logger = Logger::get();

    // -------------------------
    // 1. Initialize Device (D3D12 + NVRHI)
    // -------------------------
    Device device;
    if (!device.initialize())
    {
        LOG_ERROR("Failed to initialize device!");
        return 1;
    }
    // Create NVRHI command list for ray tracing dispatch
    nvrhi::CommandListParameters cmdParams;
    nvrhi::CommandListHandle commandList = device.getDevice()->createCommandList(cmdParams);

    const uint32_t width = 1920;
    const uint32_t height = 1080;
    // Create window with configuration
    Window::desc windowDesc;
    windowDesc.width = width;
    windowDesc.height = height;
    windowDesc.title = "007Renderer";
    windowDesc.enableVSync = false;

    Window window(device.getD3D12Device(), device.getCommandQueue(), windowDesc);
    window.PrepareResources();

    {
        // -------------------------
        // 3. Prepare shader buffer data
        // -------------------------
        struct PerFrameCB
        {
            uint32_t gWidth;
            uint32_t gHeight;
            float gColor;
            float _padding;
        } perFrameData;

        Camera camera(width, height, float3(0.f, 0.f, 1.f), float3(0.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), glm::radians(45.0f));
        Buffer cbPerFrame, cbCamera;
        Texture textureOut;
        cbPerFrame.initialize(
            device.getDevice(), &perFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB"
        );
        textureOut.initialize(
            device.getDevice(), width, height, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, true, "TextureOut"
        );
        cbCamera.initialize(
            device.getDevice(), &camera.getCameraData(), sizeof(CameraData), nvrhi::ResourceStates::ConstantBuffer, false, true, "Camera"
        );

        // -------------------------
        // 4. Setup shader & dispatch
        // -------------------------
        std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>> resourceMap;
        resourceMap["result"] = nvrhi::ResourceHandle(textureOut.getHandle().operator->());
        resourceMap["PerFrameCB"] = nvrhi::ResourceHandle(cbPerFrame.getHandle().operator->());
        resourceMap["gCamera"] = nvrhi::ResourceHandle(cbCamera.getHandle().operator->());

        nvrhi::rt::AccelStructDesc asDesc;
        nvrhi::rt::AccelStructHandle accelStruct = device.getDevice()->createAccelStruct(asDesc);
        resourceMap["gScene"] = nvrhi::ResourceHandle(accelStruct.operator->());
        // ComputePass pass;
        // pass.initialize(device.getDevice(), "/shaders/hello.slang", "computeMain", resourceMap);

        RayTracingPass pass;
        pass.initialize(device.getDevice(), "/shaders/raytracing.slang", {"rayGenMain", "missMain", "closestHitMain"}, resourceMap);

        // -------------------------
        // 5. Setup GUI with original ImGui
        // -------------------------
        bool notDone = true;
        static float gColorSlider = 1.0f; // UI slider value
        static int counter = 0;

        while (notDone)
        {
            HRESULT deviceRemovedReason = device.getD3D12Device()->GetDeviceRemovedReason();
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

            device.getDevice()->runGarbageCollection();

            perFrameData.gWidth = width;
            perFrameData.gHeight = height;
            perFrameData.gColor = gColorSlider;
            cbPerFrame.updateData(device.getDevice(), &perFrameData, sizeof(PerFrameCB));
            cbCamera.updateData(device.getDevice(), &camera.getCameraData(), sizeof(CameraData));
            commandList->open();

            // Set up resource states for buffers
            commandList->beginTrackingBufferState(cbPerFrame.getHandle(), nvrhi::ResourceStates::ConstantBuffer);
            commandList->beginTrackingBufferState(cbCamera.getHandle(), nvrhi::ResourceStates::ConstantBuffer);

            // Critical: Set up texture state for UAV access in ray tracing
            commandList->beginTrackingTextureState(textureOut.getHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            commandList->setTextureState(textureOut.getHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            // Commit barriers before dispatch
            commandList->commitBarriers();

            pass.dispatch(commandList, width, height, 1);

            commandList->close();
            nvrhi::ICommandList* commandLists[] = {commandList};
            device.getDevice()->executeCommandLists(commandLists, 1, nvrhi::CommandQueue::Graphics);

            // Set texture for display
            ID3D12Resource* d3d12Texture = static_cast<ID3D12Resource*>(textureOut.getHandle()->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource));
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
            GUI::Text("This is some useful text.");
            GUI::SliderFloat("gColor", &gColorSlider, 0.0f, 1.0f);
            if (GUI::Button("Button"))
                counter++;
            GUI::SameLine();
            GUI::Text("counter = %d", counter);
            GUI::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / GUI::GetIO().Framerate, GUI::GetIO().Framerate);
            camera.renderUI();
            camera.handleInput();
            GUI::End();

            if (GUI::IsKeyPressed(ImGuiKey_Escape))
                notDone = false; // Exit on Escape key

            // Finish rendering
            window.RenderEnd();
        }
    }

    window.CleanupResources();
    device.shutdown();
    LOG_INFO("Renderer shutdown successfully.");
    spdlog::shutdown();
    return 0;
}