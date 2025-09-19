#pragma once
#include <imgui.h>

#include "RenderPasses/RenderPassFlags.h"

// Custom ImGui wrapper
namespace GUI
{
    // Global state management
    void initialize();
    void shutdown();
    
    // Flag management
    RenderPassRefreshFlags getAndClearRefreshFlags();
    void setRefreshFlag(RenderPassRefreshFlags flag);
    bool hasRefreshFlags();
    
    // === Commonly Used Interactive Widgets (with flag tracking) ===
    
    // Sliders that trigger RenderOptionsChanged when modified
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    
    // Drag controls that trigger RenderOptionsChanged when modified
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
      // Other interactive controls
    bool Checkbox(const char* label, bool* v);
    bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1);
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
    
    // Radio buttons that trigger RenderOptionsChanged when modified
    bool RadioButton(const char* label, bool active);
    bool RadioButton(const char* label, int* v, int v_button);
    
    // === Pass-through Functions (commonly used, no flag tracking) ===
    // Text display functions
    inline void Text(const char* fmt, ...) { IM_FMTARGS(1); va_list args; va_start(args, fmt); ImGui::TextV(fmt, args); va_end(args); }
    inline void TextDisabled(const char* fmt, ...) { IM_FMTARGS(1); va_list args; va_start(args, fmt); ImGui::TextDisabledV(fmt, args); va_end(args); }
    
    // Basic controls (read-only, no modification tracking needed)
    inline bool Button(const char* label, const ImVec2& size = ImVec2(0, 0)) { return ImGui::Button(label, size); }
    inline void Image(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0)) { ImGui::Image(user_texture_id, size, uv0, uv1, tint_col, border_col); }
      
    // Layout controls
    inline bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0) { return ImGui::CollapsingHeader(label, flags); }
    inline void Separator() { ImGui::Separator(); }
    inline void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f) { ImGui::SameLine(offset_from_start_x, spacing); }
    inline void Dummy(const ImVec2& size) { ImGui::Dummy(size); }
    
    // Group controls
    inline void BeginGroup() { ImGui::BeginGroup(); }
    inline void EndGroup() { ImGui::EndGroup(); }
    
    // Window controls  
    inline bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0) { return ImGui::BeginChild(str_id, size, child_flags, window_flags); }
    inline void EndChild() { ImGui::EndChild(); }
      
    // Input/IO access functions (read-only, no tracking needed)
    inline ImGuiIO& GetIO() { return ImGui::GetIO(); }
    inline bool IsKeyDown(ImGuiKey key) { return ImGui::IsKeyDown(key); }
    inline bool IsKeyPressed(ImGuiKey key) { return ImGui::IsKeyPressed(key); }
    inline bool IsMouseDown(ImGuiMouseButton button) { return ImGui::IsMouseDown(button); }
    
    // Style and ID management
    inline void PushStyleColor(ImGuiCol idx, ImU32 col) { ImGui::PushStyleColor(idx, col); }
    inline void PushStyleColor(ImGuiCol idx, const ImVec4& col) { ImGui::PushStyleColor(idx, col); }
    inline void PopStyleColor(int count = 1) { ImGui::PopStyleColor(count); }
    inline void PushID(const char* str_id) { ImGui::PushID(str_id); }
    inline void PushID(int int_id) { ImGui::PushID(int_id); }
    inline void PopID() { ImGui::PopID(); }
    
    // Item state queries
    inline bool IsItemHovered(ImGuiHoveredFlags flags = 0) { return ImGui::IsItemHovered(flags); }
    inline void SetTooltip(const char* fmt, ...) { IM_FMTARGS(1); va_list args; va_start(args, fmt); ImGui::SetTooltipV(fmt, args); va_end(args); }
    
    // Settings management
    inline void SaveIniSettingsToDisk(const char* ini_filename) { ImGui::SaveIniSettingsToDisk(ini_filename); }
}
