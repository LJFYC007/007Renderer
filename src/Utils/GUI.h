#pragma once
#include <imgui.h>
#include <imgui_node_editor.h>
#include <memory>
#include <nvrhi/nvrhi.h>

#include "Core/Pointer.h"
#include "RenderPasses/RenderPassFlags.h"
#include "Utils/Math/Math.h"

class Device;
class Scene;
class RenderGraph;
class RenderGraphEditor;
class Window;

namespace ed = ax::NodeEditor;

// Wrapped ImGui widgets that set ResetAccumulation on the active refresh flags
// when modified inside a ScopedAccumulationReset scope. Pass-through helpers
// (Text, Button, ...) are provided so call sites can uniformly use GUI::*.
namespace GUI
{
void initialize();
void shutdown();

RenderPassRefreshFlags getRefreshFlags();
void clearRefreshFlags();
void setRefreshFlag(RenderPassRefreshFlags flag);
bool hasRefreshFlags();

void pushAccumulationResetScope(bool enabled);
void popAccumulationResetScope();
bool isAccumulationResetScopeEnabled();

class ScopedAccumulationReset
{
public:
    explicit ScopedAccumulationReset(bool enabled) { pushAccumulationResetScope(enabled); }
    ~ScopedAccumulationReset() { popAccumulationResetScope(); }
};

bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);

bool DragFloat(
    const char* label,
    float* v,
    float v_speed = 1.0f,
    float v_min = 0.0f,
    float v_max = 0.0f,
    const char* format = "%.3f",
    ImGuiSliderFlags flags = 0
);
bool DragFloat3(
    const char* label,
    float v[3],
    float v_speed = 1.0f,
    float v_min = 0.0f,
    float v_max = 0.0f,
    const char* format = "%.3f",
    ImGuiSliderFlags flags = 0
);

bool Checkbox(const char* label, bool* v);
bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1);
bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);

bool RadioButton(const char* label, bool active);
bool RadioButton(const char* label, int* v, int v_button);

inline void Text(const char* fmt, ...) IM_FMTARGS(1)
{
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}
inline void TextDisabled(const char* fmt, ...) IM_FMTARGS(1)
{
    va_list args;
    va_start(args, fmt);
    ImGui::TextDisabledV(fmt, args);
    va_end(args);
}

inline bool Button(const char* label, const ImVec2& size = ImVec2(0, 0))
{
    return ImGui::Button(label, size);
}

inline void Separator()
{
    ImGui::Separator();
}
inline void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f)
{
    ImGui::SameLine(offset_from_start_x, spacing);
}
inline void Dummy(const ImVec2& size)
{
    ImGui::Dummy(size);
}

inline void BeginGroup()
{
    ImGui::BeginGroup();
}
inline void EndGroup()
{
    ImGui::EndGroup();
}

inline bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0)
{
    return ImGui::BeginChild(str_id, size, child_flags, window_flags);
}
inline void EndChild()
{
    ImGui::EndChild();
}

inline ImGuiIO& GetIO()
{
    return ImGui::GetIO();
}
inline bool IsKeyDown(ImGuiKey key)
{
    return ImGui::IsKeyDown(key);
}
inline bool IsKeyPressed(ImGuiKey key)
{
    return ImGui::IsKeyPressed(key);
}
inline bool IsMouseDown(ImGuiMouseButton button)
{
    return ImGui::IsMouseDown(button);
}

inline void PushID(const char* str_id)
{
    ImGui::PushID(str_id);
}
inline void PushID(int int_id)
{
    ImGui::PushID(int_id);
}
inline void PopID()
{
    ImGui::PopID();
}

inline bool IsItemHovered(ImGuiHoveredFlags flags = 0)
{
    return ImGui::IsItemHovered(flags);
}
inline void SetTooltip(const char* fmt, ...) IM_FMTARGS(1)
{
    va_list args;
    va_start(args, fmt);
    ImGui::SetTooltipV(fmt, args);
    va_end(args);
}

inline void SaveIniSettingsToDisk(const char* ini_filename)
{
    ImGui::SaveIniSettingsToDisk(ini_filename);
}
} // namespace GUI

class GUIManager
{
public:
    struct LayoutConfig
    {
        float splitterWidth = 450.0f;
        float editorHeight = 500.0f;

        static constexpr float kMinSplitterWidth = 200.0f;
        static constexpr float kMinEditorHeight = 100.0f;
        static constexpr float kSplitterThickness = 8.0f;
    };

    enum class Axis
    {
        Vertical,
        Horizontal
    };

    GUIManager(ref<Device> device) : mpDevice(device) {}
    ~GUIManager() {}

    void renderMainLayout(ref<Scene> scene, RenderGraphEditor* pRenderGraphEditor, nvrhi::TextureHandle image, Window& window);

    void renderSettingsPanel(ref<Scene> scene, RenderGraphEditor* pRenderGraphEditor, nvrhi::TextureHandle image, Window& window);

    /// Draw the viewport heading and texture. Returns the actual pixel size of
    /// the image region so the caller can keep the camera in sync with it.
    /// `sceneName` and `frameCount` are shown in the viewport heading when non-empty / non-zero.
    uint2 renderRenderingPanel(ImTextureID textureId, uint32_t cameraWidth, uint32_t cameraHeight, const char* sceneName, uint32_t frameCount);

    const LayoutConfig& getLayoutConfig() const { return mLayoutConfig; }

    // Target image area the viewport panel should match on the first frame.
    // The OS window is resized once at startup so that, after layout overhead,
    // the rendering panel measures exactly this size.
    static constexpr uint32_t kInitialImageWidth = 1920;
    static constexpr uint32_t kInitialImageHeight = 1080;

private:
    /// Draggable splitter. Updates `position` in-place, clamped to `[minPos, maxPos]`.
    void splitter(Axis axis, const char* id, float& position, float minPos, float maxPos, const char* tooltip);

    ref<Device> mpDevice;
    LayoutConfig mLayoutConfig;

    // Set to true after the first frame's one-shot window resize to match kInitialImage*.
    bool mInitialViewportCalibrated = false;
};
