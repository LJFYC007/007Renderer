#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "Core/Buffer.h"
#include "Core/Device.h"
#include "Core/ComputePass.h"
#include "Utils/Logger.h"
#include "Environment.h"

class ComputeShaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        device = BasicTestEnvironment::getDevice();
        ASSERT_NE(device, nullptr);
        ASSERT_TRUE(device->isValid());
    }

    Device* device = nullptr;
};

TEST_F(ComputeShaderTest, Basic)
{
    const uint32_t elementCount = 1000; 
    std::vector<float> inputA(elementCount, 0.1f);
    std::vector<float> inputB(elementCount, 0.5f);

    Buffer bufA, bufB, bufResult;
    bufA.initialize(device->getDevice(), inputA.data(), inputA.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, false, "BufferA");
    bufB.initialize(device->getDevice(), inputB.data(), inputB.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, false, "BufferB");
    bufResult.initialize(device->getDevice(), nullptr, inputA.size() * sizeof(float), nvrhi::ResourceStates::UnorderedAccess, true, false, "BufferResult");

    std::unordered_map<std::string, nvrhi::ResourceHandle> resourceMap;
    resourceMap["BufferA"] = bufA.getHandle();
    resourceMap["BufferB"] = bufB.getHandle();
    resourceMap["BufferResult"] = bufResult.getHandle();
    
    ComputePass pass;
    pass.initialize(device->getDevice(), "/tests/ComputeShaderTest.slang", "computeMain", resourceMap);
    pass.dispatchThreads(device->getDevice(), elementCount, 1, 1);

    auto readResult = bufResult.readback(device->getDevice(), device->getDevice()->createCommandList());
    const float* resultData = reinterpret_cast<const float*>(readResult.data());
    for (size_t i = 0; i < elementCount; i++)
        EXPECT_FLOAT_EQ(resultData[i], inputA[i] + inputB[i]);
}