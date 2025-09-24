#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "Core/Device.h"
#include "ShaderPasses/ComputePass.h"
#include "Utils/ResourceIO.h"
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
    const size_t bufferByteSize = inputA.size() * sizeof(float);

    auto deviceHandle = mpDevice->getDevice();

    nvrhi::BufferDesc bufferDesc;
    bufferDesc.byteSize = bufferByteSize;
    bufferDesc.structStride = sizeof(float);
    bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    bufferDesc.keepInitialState = true;
    bufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;

    bufferDesc.debugName = "BufferA";
    nvrhi::BufferHandle bufA = deviceHandle->createBuffer(bufferDesc);
    ASSERT_TRUE(bufA);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, bufA, inputA.data(), bufferByteSize));

    bufferDesc.debugName = "BufferB";
    nvrhi::BufferHandle bufB = deviceHandle->createBuffer(bufferDesc);
    ASSERT_TRUE(bufB);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, bufB, inputB.data(), bufferByteSize));

    nvrhi::BufferDesc resultDesc = bufferDesc;
    resultDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    resultDesc.canHaveUAVs = true;
    resultDesc.debugName = "BufferResult";
    nvrhi::BufferHandle bufResult = deviceHandle->createBuffer(resultDesc);
    ASSERT_TRUE(bufResult);

    ref<Pass> pass = make_ref<ComputePass>(mpDevice, "/tests/ComputeShaderTest.slang", "computeMain");
    (*pass)["BufferA"] = bufA;
    (*pass)["BufferB"] = bufB;
    (*pass)["BufferResult"] = bufResult;
    pass->execute(elementCount, 1, 1);

    std::vector<float> resultData(elementCount);
    ASSERT_TRUE(ResourceIO::readbackBuffer(mpDevice, bufResult, resultData.data(), bufferByteSize));
    for (size_t i = 0; i < elementCount; i++)
        EXPECT_FLOAT_EQ(resultData[i], inputA[i] + inputB[i]);
}