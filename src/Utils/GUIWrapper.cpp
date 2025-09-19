#include "GUIWrapper.h"

namespace GUI
{
// Global state
static RenderPassRefreshFlags sRefreshFlags = RenderPassRefreshFlags::None;

void initialize()
{
    sRefreshFlags = RenderPassRefreshFlags::None;
}

void shutdown()
{
    sRefreshFlags = RenderPassRefreshFlags::None;
}

RenderPassRefreshFlags getAndClearRefreshFlags()
{
    RenderPassRefreshFlags result = sRefreshFlags;
    sRefreshFlags = RenderPassRefreshFlags::None;
    return result;
}

void setRefreshFlag(RenderPassRefreshFlags flag)
{
    sRefreshFlags = static_cast<RenderPassRefreshFlags>(static_cast<uint32_t>(sRefreshFlags) | static_cast<uint32_t>(flag));
}

bool hasRefreshFlags()
{
    return sRefreshFlags != RenderPassRefreshFlags::None;
}

// === Interactive Widgets with Flag Tracking ===

bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool Checkbox(const char* label, bool* v)
{
    bool changed = ImGui::Checkbox(label, v);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items)
{
    bool changed = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return changed;
}

bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags)
{
    bool changed = ImGui::ColorEdit3(label, col, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::LightingChanged);
    return changed;
}

bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags)
{
    bool changed = ImGui::ColorEdit4(label, col, flags);
    if (changed)
        setRefreshFlag(RenderPassRefreshFlags::LightingChanged);
    return changed;
}

bool RadioButton(const char* label, bool active)
{
    bool clicked = ImGui::RadioButton(label, active);
    if (clicked)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return clicked;
}

bool RadioButton(const char* label, int* v, int v_button)
{
    bool clicked = ImGui::RadioButton(label, v, v_button);
    if (clicked)
        setRefreshFlag(RenderPassRefreshFlags::RenderOptionsChanged);
    return clicked;
}
} // namespace GUI
