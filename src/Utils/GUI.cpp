#include <nvrhi/nvrhi.h>
#include <algorithm>
#include <vector>

#include "GUI.h"
#include "Theme.h"
#include "Widgets.h"
#include "ExrUtils.h"
#include "Logger.h"
#include "Core/Device.h"
#include "Core/Window.h"
#include "Scene/Scene.h"
#include "Scene/Camera/Camera.h"
#include "RenderPasses/RenderGraphEditor.h"
#include "RenderPasses/AccumulatePass/Accumulate.h"

namespace GUI
{
namespace
{
RenderPassRefreshFlags sRefreshFlags = RenderPassRefreshFlags::None;
std::vector<uint8_t> sAccumulationResetScopeStack;

void trackAccumulationReset(bool changed)
{
    if (changed && isAccumulationResetScopeEnabled())
        setRefreshFlag(RenderPassRefreshFlags::ResetAccumulation);
}
} // namespace

void initialize()
{
    sRefreshFlags = RenderPassRefreshFlags::None;
    sAccumulationResetScopeStack.clear();
}

void shutdown()
{
    sRefreshFlags = RenderPassRefreshFlags::None;
    sAccumulationResetScopeStack.clear();
}

RenderPassRefreshFlags getRefreshFlags()
{
    return sRefreshFlags;
}

void clearRefreshFlags()
{
    sRefreshFlags = RenderPassRefreshFlags::None;
}

void setRefreshFlag(RenderPassRefreshFlags flag)
{
    sRefreshFlags = static_cast<RenderPassRefreshFlags>(static_cast<uint32_t>(sRefreshFlags) | static_cast<uint32_t>(flag));
}

bool hasRefreshFlags()
{
    return sRefreshFlags != RenderPassRefreshFlags::None;
}

void pushAccumulationResetScope(bool enabled)
{
    sAccumulationResetScopeStack.push_back(enabled);
}

void popAccumulationResetScope()
{
    if (!sAccumulationResetScopeStack.empty())
        sAccumulationResetScopeStack.pop_back();
}

bool isAccumulationResetScopeEnabled()
{
    return !sAccumulationResetScopeStack.empty() && sAccumulationResetScopeStack.back();
}

bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool Checkbox(const char* label, bool* v)
{
    bool changed = ImGui::Checkbox(label, v);
    trackAccumulationReset(changed);
    return changed;
}

bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items)
{
    bool changed = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
    trackAccumulationReset(changed);
    return changed;
}

bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags)
{
    bool changed = ImGui::ColorEdit3(label, col, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags)
{
    bool changed = ImGui::ColorEdit4(label, col, flags);
    trackAccumulationReset(changed);
    return changed;
}

bool RadioButton(const char* label, bool active)
{
    bool clicked = ImGui::RadioButton(label, active);
    trackAccumulationReset(clicked);
    return clicked;
}

bool RadioButton(const char* label, int* v, int v_button)
{
    bool clicked = ImGui::RadioButton(label, v, v_button);
    trackAccumulationReset(clicked);
    return clicked;
}
} // namespace GUI

void GUIManager::splitter(Axis axis, const char* id, float& position, float minPos, float maxPos, const char* tooltip)
{
    const float thickness = LayoutConfig::kSplitterThickness;
    const ImVec2 size = (axis == Axis::Vertical) ? ImVec2(thickness, -1) : ImVec2(-1, thickness);

    // Transparent background — the grip dots are the only visual cue.
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::Button(id, size);
    ImGui::PopStyleColor(3);

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    if (active)
    {
        const float delta = (axis == Axis::Vertical) ? ImGui::GetIO().MouseDelta.x : -ImGui::GetIO().MouseDelta.y;
        position = std::clamp(position + delta, minPos, maxPos);
    }

    // Draw grip dots when hovered or dragging.
    if (hovered || active)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        const ImU32 dotColor = ImGui::GetColorU32(active ? Theme::Luminograph::kTeal : Theme::Luminograph::kInkMuted);
        constexpr int kDotCount = 3;
        constexpr float kDotRadius = 1.5f;
        constexpr float kDotSpacing = 6.0f;

        const float cx = (rmin.x + rmax.x) * 0.5f;
        const float cy = (rmin.y + rmax.y) * 0.5f;

        for (int i = 0; i < kDotCount; ++i)
        {
            const float offset = (i - (kDotCount - 1) * 0.5f) * kDotSpacing;
            if (axis == Axis::Vertical)
                dl->AddCircleFilled(ImVec2(cx, cy + offset), kDotRadius, dotColor);
            else
                dl->AddCircleFilled(ImVec2(cx + offset, cy), kDotRadius, dotColor);
        }
    }

    ImGui::SetItemTooltip("%s", tooltip);
}

void GUIManager::renderMainLayout(ref<Scene> scene, RenderGraphEditor* pRenderGraphEditor, nvrhi::TextureHandle image, Window& window)
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(
        "MainWindow",
        nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    ImGui::PopStyleVar();

    const float headerHeight = Widgets::headerHeight();
    const uint64_t sceneTris = scene ? scene->getTriangleCount() : 0ull;
    const uint64_t gpuMemMB = mpDevice ? mpDevice->getVideoMemoryUsageMB() : 0ull;
    Widgets::headerStrip({io.DeltaTime, io.Framerate, sceneTris, gpuMemMB});

    // Content = TopRow + hsplitter + Editor. Each child consumes `size + ItemSpacing.y`
    // of vertical cursor, and the trailing ItemSpacing still advances after the last
    // child — so TopRow is sized with three ItemSpacing.y values subtracted.
    const float contentPadding = 8.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(contentPadding, contentPadding));
    ImGui::BeginChild(
        "Content", ImVec2(0, io.DisplaySize.y - headerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
    const float topPanelsHeight =
        ImGui::GetContentRegionAvail().y - mLayoutConfig.editorHeight - LayoutConfig::kSplitterThickness - 3.0f * itemSpacingY;

    // Top row: settings + viewport
    ImGui::BeginChild("TopRow", ImVec2(0, topPanelsHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginChild("Settings", ImVec2(mLayoutConfig.splitterWidth, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
    renderSettingsPanel(scene, pRenderGraphEditor, image, window);
    ImGui::EndChild();

    ImGui::SameLine();
    splitter(
        Axis::Vertical,
        "##vsplitter",
        mLayoutConfig.splitterWidth,
        LayoutConfig::kMinSplitterWidth,
        io.DisplaySize.x - 400.0f,
        "Drag to resize panels"
    );

    // Keep cursor on the same row as Settings + splitter before measuring the
    // remaining width — without SameLine(), Button advances the cursor to a new
    // line and GetContentRegionAvail().x returns the full row width, so the
    // Rendering child gets a constant width and never reacts to splitter drags.
    ImGui::SameLine();
    const float rightPanelWidth = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild(
        "Rendering", ImVec2(rightPanelWidth, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    const uint32_t camW = (scene && scene->camera) ? scene->camera->getWidth() : 0;
    const uint32_t camH = (scene && scene->camera) ? scene->camera->getHeight() : 0;

    // Gather viewport metadata from the scene and render graph.
    const char* sceneName = (scene && !scene->name.empty()) ? scene->name.c_str() : nullptr;
    ref<RenderGraph> rg = pRenderGraphEditor->getCurrentRenderGraph();
    auto accPass = rg ? rg->getPassByName<AccumulatePass>("Accumulate") : nullptr;
    const uint32_t frameCount = accPass ? accPass->getFrameCount() : 0;

    const uint2 measuredImage = renderRenderingPanel(window.GetDisplayTextureImGuiHandle(), camW, camH, sceneName, frameCount);

    if (scene && scene->camera && (camW != measuredImage.x || camH != measuredImage.y))
    {
        scene->camera->setWidth(measuredImage.x);
        scene->camera->setHeight(measuredImage.y);
    }

    // One-shot calibration: snap the OS window so the panel measures exactly
    // kInitialImage{Width,Height}. Layout overhead is absorbed into window chrome.
    if (!mInitialViewportCalibrated)
    {
        const int32_t deltaW = static_cast<int32_t>(kInitialImageWidth) - static_cast<int32_t>(measuredImage.x);
        const int32_t deltaH = static_cast<int32_t>(kInitialImageHeight) - static_cast<int32_t>(measuredImage.y);
        if (deltaW != 0 || deltaH != 0)
        {
            const uint2 client = window.GetWindowSize();
            const uint32_t newClientW = static_cast<uint32_t>(static_cast<int32_t>(client.x) + deltaW);
            const uint32_t newClientH = static_cast<uint32_t>(static_cast<int32_t>(client.y) + deltaH);
            window.ResizeClientArea(newClientW, newClientH);
        }
        mInitialViewportCalibrated = true;
    }

    ImGui::EndChild();

    ImGui::EndChild(); // TopRow

    // Horizontal splitter between top panels and editor
    splitter(
        Axis::Horizontal,
        "##hsplitter",
        mLayoutConfig.editorHeight,
        LayoutConfig::kMinEditorHeight,
        io.DisplaySize.y - 200.0f,
        "Drag to resize editor panel"
    );

    // Bottom: node editor
    ImGui::BeginChild(
        "Editor", ImVec2(0, mLayoutConfig.editorHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    if (pRenderGraphEditor)
        pRenderGraphEditor->renderNodeEditor();
    ImGui::EndChild();

    ImGui::EndChild(); // Content
    ImGui::PopStyleVar();

    ImGui::End();
}

void GUIManager::renderSettingsPanel(ref<Scene> scene, RenderGraphEditor* pRenderGraphEditor, nvrhi::TextureHandle image, Window& window)
{
    // Reserve trailing space for the right-side labels so they don't get
    // clipped by the panel border.
    ImGui::PushItemWidth(-120.0f * Widgets::dpiScale());

    if (Widgets::sectionHeader("Output Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ref<RenderGraph> renderGraph = pRenderGraphEditor->getCurrentRenderGraph();
        renderGraph->renderOutputSelectionUI();

        if (GUI::Button("Save image"))
        {
            ID3D12Resource* currentTexture = window.GetCurrentDisplayTexture();
            if (currentTexture)
                ExrUtils::saveTextureToExr(mpDevice, image, std::string(PROJECT_DIR) + "/output.exr");
            else
                LOG_ERROR("No texture available to save");
        }
    }

    if (Widgets::sectionHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (scene && scene->camera)
        {
            GUI::ScopedAccumulationReset scope(true);
            scene->camera->renderUI();
            scene->camera->handleInput();
        }
    }

    if (pRenderGraphEditor)
        pRenderGraphEditor->renderUI();

    ImGui::PopItemWidth();
}

uint2 GUIManager::renderRenderingPanel(ImTextureID textureId, uint32_t cameraWidth, uint32_t cameraHeight, const char* sceneName, uint32_t frameCount)
{
    // Viewport heading: scene name | resolution | frame count
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Luminograph::kInkMuted);
        if (sceneName && sceneName[0])
            ImGui::Text("%s", sceneName);
        else
            ImGui::Text("(no scene)");
        ImGui::SameLine();
        ImGui::Text("  %u x %u", cameraWidth, cameraHeight);
        if (frameCount > 0)
        {
            ImGui::SameLine();
            ImGui::Text("  frame %u", frameCount);
        }
        ImGui::PopStyleColor();
    }

    // Accumulation progress bar (thin teal line under the heading)
    if (frameCount > 0)
    {
        // Show a continuously-filling bar — clamp to 1.0 so it saturates
        // rather than wrapping. With no fixed target, 4096 is a sensible
        // visual cap for interactive use.
        constexpr uint32_t kVisualCap = 4096;
        const float progress = (std::min)(static_cast<float>(frameCount) / kVisualCap, 1.0f);
        Widgets::accumulationBar(progress, 2.0f);
    }
    else
    {
        ImGui::Spacing();
    }

    GUI::Separator();

    // The measured content region is the single source of truth for render
    // resolution — 1:1 mapping, no aspect fit; the caller retargets the camera.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const uint32_t imageW = static_cast<uint32_t>((std::max)(avail.x, 16.0f));
    const uint32_t imageH = static_cast<uint32_t>((std::max)(avail.y, 16.0f));

    if (textureId)
    {
        const ImVec2 displaySize(static_cast<float>(imageW), static_cast<float>(imageH));
        ImGui::Image(textureId, displaySize);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
            ImGui::SetNextFrameWantCaptureMouse(false);
    }
    else
    {
        GUI::Text("No texture to display");
    }

    return uint2(imageW, imageH);
}
