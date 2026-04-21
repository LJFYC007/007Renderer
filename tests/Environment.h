#pragma once
#include <gtest/gtest.h>

#include <spdlog/sinks/ringbuffer_sink.h>

#include "Core/Device.h"
#include "Core/Pointer.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"
#include "Utils/ResourceIO.h"

// Global device instance for all tests
class BasicTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override;
    void TearDown() override;

    static ref<Device> getDevice() { return sDevice; }
    static ref<spdlog::sinks::ringbuffer_sink_mt> getLogSink() { return sLogSink; }

private:
    static ref<Device> sDevice;
    static ref<spdlog::sinks::ringbuffer_sink_mt> sLogSink;
};
