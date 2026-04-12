#pragma once
#include <imgui.h>

/*
    Luminograph instrument widgets — header strip, section headers, sparkline,
    and typographic helpers. All widgets draw with the Theme::Luminograph palette.
*/
namespace Widgets
{
/// Height the header strip consumes at the current font size / DPI.
/// Callers use this for layout math (e.g. Content area height).
float headerHeight();

/// Top strip: wordmark on the left, live frame time + wide sparkline on the right.
/// Height is derived from the current font size. Internally maintains a rolling
/// frame-time history fed one sample per call.
void headerStrip(float deltaSeconds, float fps);

/// Draw a thin line chart into an explicit screen rectangle. No cursor manipulation.
void sparkline(ImDrawList* dl, ImVec2 topLeft, ImVec2 size, const float* values, int count, int offset, ImU32 lineColor, ImU32 bgColor);

/// Collapsing header with a teal left rule and a larger font for the title.
/// Label is rendered as-is (no uppercase transform).
bool sectionHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

/// Mid-size label for sub-sections inside a section body.
void subHeader(const char* label);

/// Thin progress bar drawn at the cursor position (accumulation progress).
void accumulationBar(float progress01, float height = 2.0f);
} // namespace Widgets
