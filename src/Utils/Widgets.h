#pragma once
#include <cstdint>
#include <imgui.h>

/*
    Luminograph instrument widgets — header strip, section headers, sparkline,
    and typographic helpers. All widgets draw with the Theme::Luminograph palette.
*/
namespace Widgets
{
struct HeaderMetrics
{
    float deltaSeconds; // last frame, in seconds
    float fps;          // averaged framerate
    uint64_t sceneTris; // unique triangle count (pre-instancing)
    uint64_t gpuMemMB;  // local VRAM currently in use, in megabytes
};

/// Multiplier for converting design-space pixels to physical pixels at the
/// current DPI. Returns 1.0 if FontScaleDpi is not yet initialized.
float dpiScale();

/// Height the header strip consumes at the current font size / DPI.
/// Callers use this for layout math (e.g. Content area height).
float headerHeight();

/// Top strip: wordmark on the left, sparkline + two-line readout on the right.
/// Height is derived from the current font size. Internally maintains a rolling
/// frame-time history fed one sample per call.
void headerStrip(const HeaderMetrics& metrics);

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
