#pragma once
#include <vector>
#include <string>

#include "Core/Device.h"
#include "Core/RenderData.h"
#include "Scene/Scene.h"
#include "Utils/GUI.h"

enum class RenderDataType
{
    Texture2D,
    Buffer,
    Unknown
};

struct RenderPassInput
{
    std::string name;
    RenderDataType type;
    bool optional = false;

    RenderPassInput(const std::string& inputName, RenderDataType inputType, bool isOptional = false)
        : name(inputName), type(inputType), optional(isOptional)
    {}
};

struct RenderPassOutput
{
    std::string name;
    RenderDataType type;

    RenderPassOutput(const std::string& outputName, RenderDataType outputType) : name(outputName), type(outputType) {}
};

class RenderPass
{
public:
    RenderPass(ref<Device> device) : m_Device(device) {};

    virtual RenderData execute(const RenderData& input = RenderData()) = 0;

    virtual void renderUI() = 0;

    virtual void setScene(ref<Scene> scene) { m_Scene = scene; }

    // Get input/output interface declarations
    virtual std::vector<RenderPassInput> getInputs() const { return {}; }
    virtual std::vector<RenderPassOutput> getOutputs() const = 0;

    // Get display name for the pass
    virtual std::string getName() const = 0;

protected:
    ref<Device> m_Device;
    ref<Scene> m_Scene;
};