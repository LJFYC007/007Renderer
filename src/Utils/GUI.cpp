#include <nvrhi/nvrhi.h>
#include <algorithm>

#include "GUI.h"
#include "ExrUtils.h"
#include "Logger.h"
#include "Core/Device.h"
#include "Core/Window.h"
#include "Scene/Scene.h"
#include "Scene/Camera/Camera.h"
#include "RenderPasses/RenderGraph.h"

GUIManager::GUIManager() : mpEditorContext(nullptr)
{
    // Initialize node editor
    ed::Config config;
    config.SettingsFile = "Simple.json";
    mpEditorContext = ed::CreateEditor(&config);

    if (!mpEditorContext)
        LOG_ERROR_RETURN("Failed to create ImGui Node Editor context");
    LOG_INFO("GUI Manager initialized successfully");
}

GUIManager::~GUIManager()
{
    ed::DestroyEditor(mpEditorContext);
}

void GUIManager::renderMainLayout(ref<Scene> scene, ref<RenderGraph> renderGraph, Window& window, uint32_t& renderWidth, uint32_t& renderHeight)
{
    ImGuiIO& io = ImGui::GetIO();

    // Create full-screen window that contains the layout
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::Begin(
        "MainWindow",
        nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    const float windowPadding = ImGui::GetStyle().WindowPadding.x;
    const float topPanelsHeight = io.DisplaySize.y - mLayoutConfig.editorHeight - windowPadding * 3 - 8.0f;

    // Top row container
    ImGui::BeginChild("TopRow", ImVec2(-1, topPanelsHeight), false, ImGuiWindowFlags_NoScrollbar);
    // Left panel - Settings
    ImGui::BeginChild("Settings", ImVec2(mLayoutConfig.splitterWidth, -1), true, ImGuiWindowFlags_NoScrollbar);
    renderSettingsPanel(scene, renderGraph, window);
    ImGui::EndChild();

    // Vertical splitter between Settings and Rendering
    ImGui::SameLine();
    ImGui::Button("##vsplitter", ImVec2(8.0f, -1));
    if (ImGui::IsItemActive())
    {
        float delta = ImGui::GetIO().MouseDelta.x;
        mLayoutConfig.splitterWidth += delta;
        // Clamp splitter position
        mLayoutConfig.splitterWidth = std::max(LayoutConfig::kMinSplitterWidth, std::min(mLayoutConfig.splitterWidth, io.DisplaySize.x - 400.0f));
    }
    ImGui::SetItemTooltip("Drag to resize panels");
    // Calculate the right panel size
    const float rightPanelWidth = io.DisplaySize.x - mLayoutConfig.splitterWidth - 8.0f - windowPadding * 2;

    // Calculate rendering dimensions based on available space
    uint32_t targetWidth = (uint32_t)rightPanelWidth;
    uint32_t targetHeight = (uint32_t)(topPanelsHeight - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y * 2);

    // Update render dimensions if changed
    updateRenderDimensions(scene, renderGraph, targetWidth, targetHeight, renderWidth, renderHeight); // Right panel - Rendering display
    ImGui::SameLine();
    ImGui::BeginChild("Rendering", ImVec2(rightPanelWidth, -1), true);
    ImTextureID textureId = window.GetDisplayTextureImGuiHandle();
    renderRenderingPanel(textureId, renderWidth, renderHeight);
    ImGui::EndChild();

    ImGui::EndChild(); // End TopRow

    // Horizontal splitter between top panels and editor
    ImGui::Button("##hsplitter", ImVec2(-1, 8.0f));
    if (ImGui::IsItemActive())
    {
        float delta = ImGui::GetIO().MouseDelta.y;
        mLayoutConfig.editorHeight -= delta; // Subtract because we want editor to grow when dragging up
        // Clamp editor height
        mLayoutConfig.editorHeight = std::max(LayoutConfig::kMinEditorHeight, std::min(mLayoutConfig.editorHeight, io.DisplaySize.y - 200.0f));
    }
    ImGui::SetItemTooltip("Drag to resize editor panel");

    // Bottom panel - Node Editor (full width)
    ImGui::BeginChild("Editor", ImVec2(-1, mLayoutConfig.editorHeight), true);
    renderEditorPanel();
    ImGui::EndChild();

    ImGui::End();
}

void GUIManager::renderSettingsPanel(ref<Scene> scene, ref<RenderGraph> renderGraph, Window& window)
{
    // Save image button
    if (ImGui::Button("Save image"))
    {
        ID3D12Resource* currentTexture = window.GetCurrentDisplayTexture();
        if (currentTexture)
        {
            // Convert D3D12Resource to nvrhi::ITexture for ExrUtils
            // This requires the nvrhi texture wrapper - we need to store this differently
            // For now, log that save was requested
            LOG_INFO("Save image requested - texture conversion needs implementation");
        }
        else
        {
            LOG_ERROR("No texture available to save");
        }
    }

    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    if (ImGui::CollapsingHeader("Camera Controls"))
    {
        if (scene && scene->camera)
        {
            scene->camera->renderUI();
            scene->camera->handleInput();
        }
    }

    if (renderGraph)
        renderGraph->renderUI();
}

void GUIManager::renderRenderingPanel(ImTextureID textureId, uint32_t renderWidth, uint32_t renderHeight)
{
    ImGui::Text("Rendering (%dx%d)", renderWidth, renderHeight);
    ImGui::Separator();

    // Display the rendered image at actual resolution (1:1 pixel mapping)
    if (textureId)
        ImGui::Image(textureId, ImVec2((float)renderWidth, (float)renderHeight));
    else
        ImGui::Text("No texture to display");
}

void GUIManager::renderEditorPanel()
{
    if (!mpEditorContext)
        return;

    ed::SetCurrentEditor(mpEditorContext);
    ed::Begin("Node Editor", ImVec2(0.0, 0.0f));

    int uniqueId = 1;
    // Start drawing nodes
    ed::BeginNode(uniqueId++);
    ImGui::Text("Node A");
    ed::BeginPin(uniqueId++, ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();
    ImGui::SameLine();
    ed::BeginPin(uniqueId++, ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();
    ed::EndNode();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void GUIManager::updateRenderDimensions(
    ref<Scene> scene,
    ref<RenderGraph> renderGraph,
    uint32_t newWidth,
    uint32_t newHeight,
    uint32_t& renderWidth,
    uint32_t& renderHeight
)
{
    if (mIsFirstFrame || (newWidth != mPrevRenderWidth) || (newHeight != mPrevRenderHeight))
    {
        renderWidth = newWidth;
        renderHeight = newHeight;

        if (scene && scene->camera)
        {
            scene->camera->setWidth(renderWidth);
            scene->camera->setHeight(renderHeight);
        }
        if (renderGraph)
            renderGraph->setScene(scene);

        if (mIsFirstFrame)
        {
            LOG_DEBUG("First frame - Rendering resolution initialized to {}x{}", renderWidth, renderHeight);
            mIsFirstFrame = false;
        }
        else
            LOG_DEBUG(
                "Panel dimensions changed - Rendering resolution updated to {}x{} (prev: {}x{})",
                renderWidth,
                renderHeight,
                mPrevRenderWidth,
                mPrevRenderHeight
            );

        mPrevRenderWidth = newWidth;
        mPrevRenderHeight = newHeight;
    }
}
