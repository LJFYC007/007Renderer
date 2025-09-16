#pragma once
#include <imgui.h>
#include <imgui_node_editor.h>
#include <memory>

#include "Core/Pointer.h"

class Device;
class Scene;
class RenderGraph;
class Window;

namespace GUI = ImGui;
namespace ed = ax::NodeEditor;

class GUIManager
{
public:
    struct LayoutConfig
    {
        float splitterWidth = 450.0f;
        float editorHeight = 300.0f;
        static constexpr float kMinSplitterWidth = 200.0f;
        static constexpr float kMinEditorHeight = 100.0f;
    };

    GUIManager();
    ~GUIManager();

    // Main layout function
    void renderMainLayout(ref<Scene> scene, ref<RenderGraph> renderGraph, Window& window, uint32_t& renderWidth, uint32_t& renderHeight);

    // Individual panel render functions
    void renderSettingsPanel(ref<Scene> scene, ref<RenderGraph> renderGraph, Window& window);

    void renderRenderingPanel(ImTextureID textureId, uint32_t renderWidth, uint32_t renderHeight);

    void renderEditorPanel();

    // Getters
    const LayoutConfig& getLayoutConfig() const { return mLayoutConfig; }

private:
    void updateRenderDimensions(
        ref<Scene> scene,
        ref<RenderGraph> renderGraph,
        uint32_t newWidth,
        uint32_t newHeight,
        uint32_t& renderWidth,
        uint32_t& renderHeight
    );

    LayoutConfig mLayoutConfig;
    ed::EditorContext* mpEditorContext;

    // Tracking for dimension changes
    uint32_t mPrevRenderWidth = 0;
    uint32_t mPrevRenderHeight = 0;
    bool mIsFirstFrame = true;
};
