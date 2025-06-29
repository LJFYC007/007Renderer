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

        Camera camera(width, height, float3(0.f, 0.f, 2.f), float3(0.f, 0.f, 0.f), glm::radians(45.0f));
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
        // 4. Setup triangle geometry for ray tracing
        // -------------------------
        // Define triangle vertices (world space coordinates)
        struct Vertex
        {
            float position[3];
            float texCoord[2];
        };

        static const Vertex triangleVertices[] = {
            //  position          texCoord
            {{0.f, 0.5f, -0.5f}, {0.5f, 0.f}},
            {{-0.5f, -0.5f, -0.5f}, {0.f, 1.f}},
            {{0.5f, -0.5f, -0.5f}, {1.f, 1.f}}
        };

        // Define triangle indices
        static const uint32_t triangleIndices[] = {0, 1, 2};
        // Create vertex buffer for acceleration structure build input
        nvrhi::BufferDesc vertexBufferDesc = nvrhi::BufferDesc()
                                                 .setByteSize(sizeof(triangleVertices))
                                                 .setIsVertexBuffer(true)
                                                 .setInitialState(nvrhi::ResourceStates::VertexBuffer)
                                                 .setKeepInitialState(true) // enable fully automatic state tracking
                                                 .setDebugName("Vertex Buffer")
                                                 .setCanHaveRawViews(true)
                                                 .setIsAccelStructBuildInput(true);
        nvrhi::BufferHandle vertexBuffer = device.getDevice()->createBuffer(vertexBufferDesc);

        // Create index buffer for acceleration structure build input
        nvrhi::BufferDesc indexBufferDesc = nvrhi::BufferDesc()
                                                .setByteSize(sizeof(triangleIndices))
                                                .setIsIndexBuffer(true)
                                                .setInitialState(nvrhi::ResourceStates::IndexBuffer)
                                                .setKeepInitialState(true) // enable fully automatic state tracking
                                                .setDebugName("Index Buffer")
                                                .setCanHaveRawViews(true)
                                                .setIsAccelStructBuildInput(true);
        nvrhi::BufferHandle indexBuffer = device.getDevice()->createBuffer(indexBufferDesc);

        // Geometry descriptor
        auto triangles = nvrhi::rt::GeometryTriangles()
                             .setVertexBuffer(vertexBuffer)
                             .setVertexFormat(nvrhi::Format::RGB32_FLOAT)
                             .setVertexCount(std::size(triangleVertices))
                             .setVertexStride(sizeof(Vertex))
                             .setIndexBuffer(indexBuffer)
                             .setIndexFormat(nvrhi::Format::R32_UINT)
                             .setIndexCount(std::size(triangleIndices)); // BLAS descriptor with proper geometry flags
        auto blasDesc = nvrhi::rt::AccelStructDesc().setDebugName("BLAS").setIsTopLevel(false).addBottomLevelGeometry(
            nvrhi::rt::GeometryDesc().setTriangles(triangles).setFlags(nvrhi::rt::GeometryFlags::Opaque)
        );

        nvrhi::rt::AccelStructHandle blas = device.getDevice()->createAccelStruct(blasDesc);

        auto tlasDesc = nvrhi::rt::AccelStructDesc().setDebugName("TLAS").setIsTopLevel(true).setTopLevelMaxInstances(1);

        nvrhi::rt::AccelStructHandle tlas = device.getDevice()->createAccelStruct(tlasDesc);
        auto commandList = device.getCommandList();
        commandList->open();

        // Upload the vertex and index data
        commandList->writeBuffer(vertexBuffer, triangleVertices, sizeof(triangleVertices));
        commandList->writeBuffer(indexBuffer, triangleIndices, sizeof(triangleIndices));

        // Build the BLAS using the geometry array populated earlier.
        // It's also possible to obtain the descriptor from the BLAS object using getDesc()
        // and write the vertex and index buffer references into that descriptor again
        // because NVRHI erases those when it creates the AS object.
        commandList->buildBottomLevelAccelStruct(blas, blasDesc.bottomLevelGeometries.data(), blasDesc.bottomLevelGeometries.size());
        auto instanceDesc = nvrhi::rt::InstanceDesc()
                                .setBLAS(blas)
                                .setFlags(nvrhi::rt::InstanceFlags::TriangleCullDisable)
                                .setTransform(nvrhi::rt::c_IdentityTransform)
                                .setInstanceMask(0xFF);

        commandList->buildTopLevelAccelStruct(tlas, &instanceDesc, 1);

        commandList->close();
        device.getDevice()->executeCommandList(commandList);

        // -------------------------
        // 5. Setup shader & dispatch
        // -------------------------
        std::unordered_map<std::string, nvrhi::ResourceHandle> resourceMap;
        resourceMap["result"] = textureOut.getHandle().operator->();
        resourceMap["PerFrameCB"] = cbPerFrame.getHandle().operator->();
        resourceMap["gCamera"] = cbCamera.getHandle().operator->();

        std::unordered_map<std::string, nvrhi::rt::AccelStructHandle> accelStructMap;
        accelStructMap["gScene"] = tlas;

        RayTracingPass pass;
        std::unordered_map<std::string, nvrhi::ShaderType> entryPoints = {
            {"rayGenMain", nvrhi::ShaderType::RayGeneration}, {"missMain", nvrhi::ShaderType::Miss}, {"closestHitMain", nvrhi::ShaderType::ClosestHit}
        };
        pass.initialize(device, "/shaders/raytracing.slang", entryPoints, resourceMap, accelStructMap);

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

            pass.dispatch(device, width, height, 1);

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