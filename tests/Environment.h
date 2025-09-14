#pragma once
#include <gtest/gtest.h>

#include "Core/Device.h"
#include "Core/Pointer.h"
#include "Utils/Logger.h"

// Global device instance for all tests
class BasicTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        Logger::init();
        Logger::get()->set_level(spdlog::level::off); // Disable logging for tests
        device = make_ref<Device>();
        if (!device->initialize())
            FAIL() << "Failed to initialize device for Buffer tests";
    }

    void TearDown() override
    {
        device.reset();
        spdlog::shutdown();
    }

    static ref<Device> getDevice() { return device; }

private:
    static ref<Device> device;
};