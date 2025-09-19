#pragma once
#include <gtest/gtest.h>

#include "Core/Device.h"
#include "Core/Pointer.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"

// Global device instance for all tests
class BasicTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        Logger::init();
        Logger::get()->set_level(spdlog::level::off); // Disable logging for tests
        sDevice = make_ref<Device>();
        if (!sDevice->initialize())
            FAIL() << "Failed to initialize device for Buffer tests";
        ImGui::CreateContext();
    }
    
    void TearDown() override
    {
        ImGui::DestroyContext();
        sDevice.reset();
        spdlog::shutdown();
    }
    
    static ref<Device> getDevice() { return sDevice; }

private:
    static ref<Device> sDevice;
};