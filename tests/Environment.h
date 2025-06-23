#include <gtest/gtest.h>

#include "Core/Device.h"
#include "Utils/Logger.h"

// Global device instance for all tests
class BasicTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        Logger::init();
        device = std::make_unique<Device>();
        if (!device->initialize()) FAIL() << "Failed to initialize device for Buffer tests";
    }

    void TearDown() override
    {
        device.reset();
        spdlog::shutdown();
    }

    static Device* getDevice() { return device.get(); }

private:
    static std::unique_ptr<Device> device;
};

std::unique_ptr<Device> BasicTestEnvironment::device = nullptr;
// Register the global test environment
::testing::Environment* const basicEnv = ::testing::AddGlobalTestEnvironment(new BasicTestEnvironment);