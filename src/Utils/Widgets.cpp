#include "Widgets.h"
#include "Theme.h"

#include <algorithm>
#include <cstdio>

namespace Widgets
{
using namespace Theme::Luminograph;

namespace
{
constexpr float kWordmarkScale = 1.70f;
constexpr float kSectionTitleScale = 1.35f;
constexpr float kSubHeaderScale = 1.10f;

constexpr int kFrameHistorySize = 240;
float sFrameHistory[kFrameHistorySize] = {};
int sFrameHistoryOffset = 0;

ImU32 toU32(const ImVec4& c)
{
    return ImGui::GetColorU32(c);
}

void drawHairline(ImDrawList* dl, ImVec2 a, ImVec2 b)
{
    dl->AddLine(a, b, toU32(kHairline), 1.0f * dpiScale());
}
} // namespace

float dpiScale()
{
    const float s = ImGui::GetStyle().FontScaleDpi;
    return s > 0.0f ? s : 1.0f;
}

void sparkline(ImDrawList* dl, ImVec2 topLeft, ImVec2 size, const float* values, int count, int offset, ImU32 lineColor, ImU32 bgColor)
{
    if (!dl || !values || count <= 1)
        return;

    const ImVec2 br(topLeft.x + size.x, topLeft.y + size.y);
    dl->AddRectFilled(topLeft, br, bgColor);
    drawHairline(dl, ImVec2(topLeft.x, br.y - 0.5f), ImVec2(br.x, br.y - 0.5f));

    float vmin = values[0];
    float vmax = values[0];
    for (int i = 1; i < count; ++i)
    {
        vmin = (std::min)(vmin, values[i]);
        vmax = (std::max)(vmax, values[i]);
    }
    if (vmax - vmin < 0.001f)
        vmax = vmin + 1.0f;

    const float innerPad = 2.0f;
    const float plotH = size.y - 2.0f * innerPad;
    const float plotW = size.x - 2.0f * innerPad;

    ImVec2 prev;
    for (int i = 0; i < count; ++i)
    {
        const int idx = (offset + i) % count;
        const float v = values[idx];
        const float t = (v - vmin) / (vmax - vmin);
        const float x = topLeft.x + innerPad + (plotW * i) / (count - 1);
        const float y = topLeft.y + innerPad + plotH * (1.0f - t);
        const ImVec2 p(x, y);
        if (i > 0)
            dl->AddLine(prev, p, lineColor, 1.2f * dpiScale());
        prev = p;
    }
}

float headerHeight()
{
    // Large enough to fit the wordmark rendered at `base * kWordmarkScale * DPI`
    // with breathing room above and below. Uses GetFontSize() so DPI is baked in.
    return ImGui::GetFontSize() * 2.4f;
}

namespace
{
void formatTriangleCount(char* out, size_t n, uint64_t tris)
{
    if (tris >= 1'000'000ull)
        std::snprintf(out, n, "%.2fM tris", static_cast<double>(tris) / 1.0e6);
    else if (tris >= 1'000ull)
        std::snprintf(out, n, "%.1fK tris", static_cast<double>(tris) / 1.0e3);
    else
        std::snprintf(out, n, "%llu tris", static_cast<unsigned long long>(tris));
}

void formatGpuMem(char* out, size_t n, uint64_t mb)
{
    if (mb >= 1024ull)
        std::snprintf(out, n, "%.2f GB", static_cast<double>(mb) / 1024.0);
    else
        std::snprintf(out, n, "%llu MB", static_cast<unsigned long long>(mb));
}
} // namespace

void headerStrip(const HeaderMetrics& metrics)
{
    sFrameHistory[sFrameHistoryOffset] = metrics.deltaSeconds * 1000.0f;
    sFrameHistoryOffset = (sFrameHistoryOffset + 1) % kFrameHistorySize;

    const float height = headerHeight();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, toU32(kPaperSunken));
    ImGui::BeginChild("##LuminoHeader", ImVec2(0, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 br(origin.x + width, origin.y + height);

    drawHairline(dl, ImVec2(origin.x, br.y - 0.5f), ImVec2(br.x, br.y - 0.5f));

    const float padX = 14.0f;
    const float baseFontSize = ImGui::GetFontSize();
    const float baseCenterY = origin.y + (height - baseFontSize) * 0.5f;

    // Wordmark is drawn via PushFont so DPI (FontScaleDpi) is applied uniformly
    // with the rest of the UI — keeps it visually the largest at any DPI.
    const float baseSize = ImGui::GetStyle().FontSizeBase;
    ImGui::PushFont(nullptr, baseSize * kWordmarkScale);
    const float wordmarkRendered = ImGui::GetFontSize();
    const float wordmarkCenterY = origin.y + (height - wordmarkRendered) * 0.5f;

    const float markSize = wordmarkRendered * 0.5f;
    const float markY = origin.y + (height - markSize) * 0.5f;
    dl->AddRectFilled(ImVec2(origin.x + padX, markY), ImVec2(origin.x + padX + markSize, markY + markSize), toU32(kTeal));

    const char* wordmark = "007Renderer";
    const ImVec2 wordPos(origin.x + padX + markSize + 10.0f, wordmarkCenterY);
    dl->AddText(ImVec2(wordPos.x, wordPos.y), toU32(kInk), wordmark);
    const float wordRightEdge = wordPos.x + ImGui::CalcTextSize(wordmark).x;
    ImGui::PopFont();

    // Single-line readout: timing in primary ink, scene/GPU stats in muted ink.
    char timingText[64];
    std::snprintf(timingText, sizeof(timingText), "%6.2f ms   %6.1f fps", metrics.deltaSeconds * 1000.0f, metrics.fps);

    char trisText[32];
    char memText[32];
    formatTriangleCount(trisText, sizeof(trisText), metrics.sceneTris);
    formatGpuMem(memText, sizeof(memText), metrics.gpuMemMB);
    char statsText[80];
    std::snprintf(statsText, sizeof(statsText), "   |   %s   %s", trisText, memText);

    const ImVec2 timingSize = ImGui::CalcTextSize(timingText);
    const ImVec2 statsSize = ImGui::CalcTextSize(statsText);

    const float gap = 16.0f;
    const float rightEdge = br.x - padX;
    const float statsX = rightEdge - statsSize.x;
    const float timingX = statsX - timingSize.x;

    dl->AddText(ImVec2(timingX, baseCenterY), toU32(kInk), timingText);
    dl->AddText(ImVec2(statsX, baseCenterY), toU32(kInkMuted), statsText);

    const float sepY0 = origin.y + 8.0f;
    const float sepY1 = br.y - 8.0f;
    drawHairline(dl, ImVec2(timingX - gap * 0.5f, sepY0), ImVec2(timingX - gap * 0.5f, sepY1));

    // Right-anchored, capped so wide windows don't stretch it.
    const float sparkHeight = height - 10.0f;
    const float sparkMaxWidth = 220.0f;
    const float sparkRight = timingX - gap;
    const float sparkLeftLimit = wordRightEdge + gap;
    const float sparkLeft = (std::max)(sparkLeftLimit, sparkRight - sparkMaxWidth);
    const float sparkWidth = sparkRight - sparkLeft;

    if (sparkWidth > 20.0f)
    {
        const float sparkY = origin.y + (height - sparkHeight) * 0.5f;
        sparkline(
            dl,
            ImVec2(sparkLeft, sparkY),
            ImVec2(sparkWidth, sparkHeight),
            sFrameHistory,
            kFrameHistorySize,
            sFrameHistoryOffset,
            toU32(kTeal),
            toU32(kTealWash)
        );
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

bool sectionHeader(const char* label, ImGuiTreeNodeFlags flags)
{
    const float baseSize = ImGui::GetStyle().FontSizeBase;
    ImGui::PushFont(nullptr, baseSize * kSectionTitleScale);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float lineHeight = ImGui::GetFrameHeight();

    const float ruleWidth = 3.0f;
    dl->AddRectFilled(ImVec2(origin.x, origin.y + 2.0f), ImVec2(origin.x + ruleWidth, origin.y + lineHeight - 2.0f), toU32(kTeal));

    ImGui::Indent(ruleWidth + 6.0f);
    const bool open = ImGui::CollapsingHeader(label, flags);
    ImGui::Unindent(ruleWidth + 6.0f);

    ImGui::PopFont();
    return open;
}

void subHeader(const char* label)
{
    const float baseSize = ImGui::GetStyle().FontSizeBase;
    ImGui::PushFont(nullptr, baseSize * kSubHeaderScale);
    ImGui::TextUnformatted(label);
    ImGui::PopFont();
}

void accumulationBar(float progress01, float height)
{
    if (progress01 < 0.0f)
        progress01 = 0.0f;
    else if (progress01 > 1.0f)
        progress01 = 1.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 br(origin.x + width, origin.y + height);

    dl->AddRectFilled(origin, br, toU32(kTealWash));
    dl->AddRectFilled(origin, ImVec2(origin.x + width * progress01, br.y), toU32(kTeal));

    ImGui::Dummy(ImVec2(width, height));
}
} // namespace Widgets
