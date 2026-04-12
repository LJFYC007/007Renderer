#include "Theme.h"
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace Theme
{
using namespace Luminograph;

void applyGlobalStyle(ImGuiStyle& style)
{
    // ---- Geometry --------------------------------------------------------
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    // Left-align titles so the text reads like an instrument label.
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.45f;

    // ---- Colors ----------------------------------------------------------
    ImVec4* c = style.Colors;

    c[ImGuiCol_Text] = kInk;
    c[ImGuiCol_TextDisabled] = kInkDisabled;

    c[ImGuiCol_WindowBg] = kPaper;
    c[ImGuiCol_ChildBg] = kPaper;
    c[ImGuiCol_PopupBg] = kPaperRaised;

    c[ImGuiCol_Border] = kHairline;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // Frame backgrounds (input fields, sliders, drags)
    c[ImGuiCol_FrameBg] = kPaperSunken;
    c[ImGuiCol_FrameBgHovered] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.10f);
    c[ImGuiCol_FrameBgActive] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.18f);

    // Title bars (mostly hidden, but set for completeness)
    c[ImGuiCol_TitleBg] = kPaperSunken;
    c[ImGuiCol_TitleBgActive] = kPaperSunken;
    c[ImGuiCol_TitleBgCollapsed] = kPaperSunken;

    c[ImGuiCol_MenuBarBg] = kPaperSunken;

    // Scrollbars
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = kHairline;
    c[ImGuiCol_ScrollbarGrabHovered] = kInkMuted;
    c[ImGuiCol_ScrollbarGrabActive] = kInk;

    // Input text caret
    c[ImGuiCol_InputTextCursor] = kInk;

    // Sliders / checkboxes / radios
    c[ImGuiCol_CheckMark] = kTeal;
    c[ImGuiCol_SliderGrab] = kTeal;
    c[ImGuiCol_SliderGrabActive] = kTealBright;

    // Buttons — subtle teal border on hover, faint teal tint at rest
    c[ImGuiCol_Button] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.06f);
    c[ImGuiCol_ButtonHovered] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.14f);
    c[ImGuiCol_ButtonActive] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.26f);

    // Headers (CollapsingHeader, Selectable)
    c[ImGuiCol_Header] = kTealWash;
    c[ImGuiCol_HeaderHovered] = kTealDim;
    c[ImGuiCol_HeaderActive] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.22f);

    // Separators
    c[ImGuiCol_Separator] = kHairline;
    c[ImGuiCol_SeparatorHovered] = kTeal;
    c[ImGuiCol_SeparatorActive] = kTealBright;

    // Resize grips
    c[ImGuiCol_ResizeGrip] = kHairline;
    c[ImGuiCol_ResizeGripHovered] = kTeal;
    c[ImGuiCol_ResizeGripActive] = kTealBright;

    // Tabs
    c[ImGuiCol_Tab] = kPaperSunken;
    c[ImGuiCol_TabHovered] = kTealDim;
    c[ImGuiCol_TabSelected] = kPaper;
    c[ImGuiCol_TabSelectedOverline] = kTeal;
    c[ImGuiCol_TabDimmed] = kPaperSunken;
    c[ImGuiCol_TabDimmedSelected] = kPaper;
    c[ImGuiCol_TabDimmedSelectedOverline] = kHairline;

    // Plots
    c[ImGuiCol_PlotLines] = kTeal;
    c[ImGuiCol_PlotLinesHovered] = kTealBright;
    c[ImGuiCol_PlotHistogram] = kTeal;
    c[ImGuiCol_PlotHistogramHovered] = kTealBright;

    // Tables
    c[ImGuiCol_TableHeaderBg] = kPaperSunken;
    c[ImGuiCol_TableBorderStrong] = kHairline;
    c[ImGuiCol_TableBorderLight] = ImVec4(kHairline.x, kHairline.y, kHairline.z, 0.5f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.04f);

    // Hyperlinks, tree outlines, unsaved marker
    c[ImGuiCol_TextLink] = kTeal;
    c[ImGuiCol_TreeLines] = kHairline;
    c[ImGuiCol_UnsavedMarker] = kAmber;

    // Selection / drag-drop / nav
    c[ImGuiCol_TextSelectedBg] = kTealDim;
    c[ImGuiCol_DragDropTarget] = kAmber;
    c[ImGuiCol_DragDropTargetBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_NavCursor] = kTeal;
    c[ImGuiCol_NavWindowingHighlight] = kTeal;
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(kInk.x, kInk.y, kInk.z, 0.20f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(kInk.x, kInk.y, kInk.z, 0.35f);
}

void applyNodeEditorStyle()
{
    auto& style = ed::GetStyle();

    // All selection/highlight states use teal. Amber is reserved for error
    // messages (Add Pass popup) — never blended over warm paper, since rust+cream
    // produces a peachy-pink tint.
    style.Colors[ed::StyleColor_Bg] = kPaper;
    style.Colors[ed::StyleColor_Grid] = kGridMinor;
    style.Colors[ed::StyleColor_NodeBg] = kPaperRaised;
    style.Colors[ed::StyleColor_NodeBorder] = kTeal;
    style.Colors[ed::StyleColor_HovNodeBorder] = kTealBright;
    style.Colors[ed::StyleColor_SelNodeBorder] = kTealBright;
    style.Colors[ed::StyleColor_PinRect] = ImVec4(kTeal.x, kTeal.y, kTeal.z, 0.85f);
    style.Colors[ed::StyleColor_PinRectBorder] = kTealBright;
    style.Colors[ed::StyleColor_HovLinkBorder] = kTealBright;
    style.Colors[ed::StyleColor_SelLinkBorder] = kTealBright;
    style.Colors[ed::StyleColor_HighlightLinkBorder] = kTealBright;
    style.Colors[ed::StyleColor_NodeSelRect] = kTealDim;
    style.Colors[ed::StyleColor_NodeSelRectBorder] = kTeal;
    style.Colors[ed::StyleColor_LinkSelRect] = kTealDim;
    style.Colors[ed::StyleColor_LinkSelRectBorder] = kTeal;
    style.Colors[ed::StyleColor_Flow] = kTeal;
    style.Colors[ed::StyleColor_FlowMarker] = kTeal;
    style.Colors[ed::StyleColor_GroupBg] = kTealWash;
    style.Colors[ed::StyleColor_GroupBorder] = kHairline;

    style.NodePadding = ImVec4(14, 10, 14, 12);
    style.NodeRounding = 2.0f;
    style.NodeBorderWidth = 1.0f;
    style.HoveredNodeBorderWidth = 2.0f;
    style.SelectedNodeBorderWidth = 2.5f;
    style.HoverNodeBorderOffset = 1.0f;
    style.SelectedNodeBorderOffset = 1.0f;

    style.PinRounding = 2.0f;
    style.PinBorderWidth = 1.0f;
    style.PinRadius = 5.0f;
    style.PinArrowSize = 0.0f;
    style.PinArrowWidth = 0.0f;

    style.LinkStrength = 150.0f;
    style.FlowMarkerDistance = 30.0f;
    style.FlowSpeed = 150.0f;
    style.FlowDuration = 2.0f;

    style.GroupRounding = 2.0f;
    style.GroupBorderWidth = 1.0f;
    style.HighlightConnectedLinks = 1.0f;
    style.SnapLinkToPinDir = 1.0f;
}
} // namespace Theme
