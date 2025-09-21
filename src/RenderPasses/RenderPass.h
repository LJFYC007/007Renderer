#pragma once
#include <vector>
#include <string>
#include <functional>
#include <algorithm>

#include "Core/Device.h"
#include "Core/RenderData.h"
#include "Scene/Scene.h"
#include "Utils/GUI.h"

#ifndef RENDERPASS_FRIEND_TEST
#if defined(__has_include)
#if __has_include(<gtest/gtest_prod.h>)
#include <gtest/gtest_prod.h>
#define RENDERPASS_FRIEND_TEST(test_case_name, test_name) FRIEND_TEST(test_case_name, test_name)
#else
#define RENDERPASS_FRIEND_TEST(test_case_name, test_name)
#endif
#else
#define RENDERPASS_FRIEND_TEST(test_case_name, test_name)
#endif
#endif

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
    RenderPass(ref<Device> pDevice) : mpDevice(pDevice) {};

    virtual RenderData execute(const RenderData& input = RenderData()) = 0;

    virtual void renderUI() = 0;

    virtual void setScene(ref<Scene> pScene) { mpScene = pScene; }

    // Get input/output interface declarations
    virtual std::vector<RenderPassInput> getInputs() const { return {}; }
    virtual std::vector<RenderPassOutput> getOutputs() const = 0;

    // Get display name for the pass
    virtual std::string getName() const = 0;

protected:
    ref<Device> mpDevice;
    ref<Scene> mpScene;
};

struct RenderPassDescriptor
{
    std::string displayName;
    std::string description;
    std::function<ref<RenderPass>(ref<Device>)> factory;
};

class RenderPassRegistry
{
public:
    static void registerPass(const RenderPassDescriptor& descriptor)
    {
        auto& registry = getRegistry();
        auto it = std::find_if(
            registry.begin(), registry.end(), [&](const RenderPassDescriptor& existing) { return existing.displayName == descriptor.displayName; }
        );

        if (it == registry.end())
            registry.push_back(descriptor);
    }

    static const std::vector<RenderPassDescriptor>& getRegisteredPasses() { return getRegistry(); }

private:
    static std::vector<RenderPassDescriptor>& getRegistry()
    {
        static std::vector<RenderPassDescriptor> sRegistry;
        return sRegistry;
    }
};
