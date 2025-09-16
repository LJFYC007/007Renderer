#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "Core/Buffer.h"
#include "Core/Device.h"
#include "ShaderPasses/ComputePass.h"
#include "Utils/Logger.h"
#include "Environment.h"

class ComputeShaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mpDevice = BasicTestEnvironment::getDevice();
        ASSERT_NE(mpDevice, nullptr);
        ASSERT_TRUE(mpDevice->isValid());
    }

    ref<Device> mpDevice;
};

TEST_F(ComputeShaderTest, Basic)
{
    const uint32_t elementCount = 1000;
    std::vector<float> inputA(elementCount, 0.1f);
    std::vector<float> inputB(elementCount, 0.5f);
    Buffer bufA, bufB, bufResult;
    bufA.initialize(mpDevice, inputA.data(), inputA.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, false, "BufferA");
    bufB.initialize(mpDevice, inputB.data(), inputB.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, false, "BufferB");
    bufResult.initialize(mpDevice, nullptr, inputA.size() * sizeof(float), nvrhi::ResourceStates::UnorderedAccess, true, false, "BufferResult");

    ref<Pass> pass = make_ref<ComputePass>(mpDevice, "/tests/ComputeShaderTest.slang", "computeMain");
    (*pass)["BufferA"] = bufA.getHandle();
    (*pass)["BufferB"] = bufB.getHandle();
    (*pass)["BufferResult"] = bufResult.getHandle();
    pass->execute(elementCount, 1, 1);
    auto readResult = bufResult.readback(mpDevice);
    const float* resultData = reinterpret_cast<const float*>(readResult.data());
    for (size_t i = 0; i < elementCount; i++)
        EXPECT_FLOAT_EQ(resultData[i], inputA[i] + inputB[i]);
}