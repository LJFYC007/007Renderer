#pragma once
#include <imgui.h>

/*
    Luminograph: a light-mode technical-instrument aesthetic.

    One ink (teal) for interaction, one alarm (amber) for warnings.
    Warm paper background, hairline borders, minimal rounding.
*/
namespace Theme
{
// Core palette. All values are sRGB-interpreted floats in [0,1].
namespace Luminograph
{
// Paper / surface
constexpr ImVec4 kPaper = ImVec4(0.961f, 0.949f, 0.918f, 1.000f);       // #F5F2EA warm off-white
constexpr ImVec4 kPaperSunken = ImVec4(0.933f, 0.918f, 0.882f, 1.000f); // recessed fields
constexpr ImVec4 kPaperRaised = ImVec4(1.000f, 0.996f, 0.980f, 1.000f); // raised cards (nodes)
constexpr ImVec4 kHairline = ImVec4(0.784f, 0.761f, 0.702f, 1.000f);    // 1px rules and borders

// Ink (text)
constexpr ImVec4 kInk = ImVec4(0.110f, 0.118f, 0.133f, 1.000f);      // #1C1E22 near-black
constexpr ImVec4 kInkMuted = ImVec4(0.392f, 0.400f, 0.420f, 1.000f); // secondary text
constexpr ImVec4 kInkDisabled = ImVec4(0.620f, 0.620f, 0.600f, 1.000f);

// Teal — the single interaction accent
constexpr ImVec4 kTeal = ImVec4(0.039f, 0.373f, 0.455f, 1.000f);       // #0A5F74
constexpr ImVec4 kTealBright = ImVec4(0.063f, 0.478f, 0.580f, 1.000f); // hover
constexpr ImVec4 kTealDim = ImVec4(0.039f, 0.373f, 0.455f, 0.140f);    // translucent fills
constexpr ImVec4 kTealWash = ImVec4(0.039f, 0.373f, 0.455f, 0.060f);   // very faint wash

// Amber — reserved for warnings / resets / errors
constexpr ImVec4 kAmber = ImVec4(0.761f, 0.341f, 0.043f, 1.000f); // #C2570B
constexpr ImVec4 kAmberDim = ImVec4(0.761f, 0.341f, 0.043f, 0.200f);

// Grid overlays for blueprint node editor
constexpr ImVec4 kGridMinor = ImVec4(0.039f, 0.373f, 0.455f, 0.080f);
constexpr ImVec4 kGridMajor = ImVec4(0.039f, 0.373f, 0.455f, 0.180f);
} // namespace Luminograph

/// Apply the Luminograph palette + geometry to the global ImGui style.
/// Call once after ImGui::CreateContext(), before ScaleAllSizes.
void applyGlobalStyle(ImGuiStyle& style);

/// Apply the blueprint-sheet style to a currently-bound imgui-node-editor context.
/// Caller is responsible for `ed::SetCurrentEditor(ctx)` before and nulling after.
void applyNodeEditorStyle();
} // namespace Theme
