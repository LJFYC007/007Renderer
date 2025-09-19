#pragma once
#include <imgui.h>
#include <imgui_node_editor.h>
#include <memory>
#include <nvrhi/nvrhi.h>

#include "Core/Pointer.h"
#include "GUIWrapper.h"

class Device;
class Scene;
class RenderGraph;
class RenderGraphEditor;
class Window;

namespace ed = ax::NodeEditor;

class GUIManager
{
public:
    struct LayoutConfig
    {
        float splitterWidth = 450.0f;
        float editorHeight = 500.0f;
        static constexpr float kMinSplitterWidth = 200.0f;
        static constexpr float kMinEditorHeight = 100.0f;
    };

    GUIManager(ref<Device> device) : mpDevice(device) {}
    ~GUIManager() {}

    // Main layout function
    void renderMainLayout(
        ref<Scene> scene,
        RenderGraphEditor* pRenderGraphEditor,
        nvrhi::TextureHandle image,
        Window& window,
        uint32_t& renderWidth,
        uint32_t& renderHeight
    );

    // Individual panel render functions
    void renderSettingsPanel(ref<Scene> scene, RenderGraphEditor* pRenderGraphEditor, nvrhi::TextureHandle image, Window& window);

    void renderRenderingPanel(ImTextureID textureId, uint32_t renderWidth, uint32_t renderHeight);

    // Getters
    const LayoutConfig& getLayoutConfig() const { return mLayoutConfig; }

private:
    void updateRenderDimensions(
        ref<Scene> scene,
        RenderGraphEditor* pRenderGraphEditor,
        uint32_t newWidth,
        uint32_t newHeight,
        uint32_t& renderWidth,
        uint32_t& renderHeight
    );

    ref<Device> mpDevice;
    LayoutConfig mLayoutConfig;

    // Tracking for dimension changes
    uint32_t mPrevRenderWidth = 0;
    uint32_t mPrevRenderHeight = 0;
    bool mIsFirstFrame = true;
};
