#pragma once
#include <gtest/gtest.h>
#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Device.h"
#include "Core/Pointer.h"
#include "Environment.h"
#include "Utils/Math/Math.h"

namespace TestHelpers
{
// Artifact directory resolution for test-produced files (e.g. output EXRs).
// Callers should save only on failure so passing runs stay hermetic.
// The path is <PROJECT_DIR>/tests/artifacts/<full-test-name>/ and is created on demand.
std::string artifactDir();
std::string artifactPath(const std::string& filename);

// Readback a buffer into a byte vector. Returns an empty vector on failure.
std::vector<uint8_t> readbackBuffer(ref<Device> device, nvrhi::BufferHandle buffer, size_t byteSize);

nvrhi::BufferHandle createStructuredBufferSRV(ref<Device> device, const void* data, size_t byteSize, size_t stride, const char* name);

nvrhi::BufferHandle createStructuredBufferUAV(ref<Device> device, size_t byteSize, size_t stride, const char* name);

nvrhi::TextureHandle createFloat4Texture1D(ref<Device> device, const float4* texels, uint32_t width, const char* name);

// Gating signals consumed by test bodies to self-skip via GTEST_SKIP():
// RENDERER_FAST_TESTS=1     → skip slow convergence tests
// RENDERER_RUN_BENCHMARKS=1 → include benchmark tests (otherwise skipped)
bool isFastMode();
bool isBenchMode();
} // namespace TestHelpers

// Base fixture for functional tests; grabs the process-lifetime device and
// self-skips when RENDERER_RUN_BENCHMARKS=1.
class DeviceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (TestHelpers::isBenchMode())
            GTEST_SKIP() << "non-benchmark test; unset RENDERER_RUN_BENCHMARKS to run";

        mpDevice = BasicTestEnvironment::getDevice();
        ASSERT_NE(mpDevice, nullptr);
        ASSERT_TRUE(mpDevice->isValid());
    }

    ref<Device> mpDevice;
};

// Benchmark fixture: runs only when RENDERER_RUN_BENCHMARKS=1. Pair with
// a TEST_F(SuiteName, ...) derived from this to self-gate benchmarks.
class BenchmarkTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!TestHelpers::isBenchMode())
            GTEST_SKIP() << "benchmark; set RENDERER_RUN_BENCHMARKS=1 to run";

        mpDevice = BasicTestEnvironment::getDevice();
        ASSERT_NE(mpDevice, nullptr);
        ASSERT_TRUE(mpDevice->isValid());
    }

    ref<Device> mpDevice;
};
