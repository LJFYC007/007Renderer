#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "Core/Buffer.h"
#include "Core/Device.h"
#include "Utils/Logger.h"
#include "Environment.h"

class BufferTest : public ::testing::Test
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

TEST_F(BufferTest, BasicInitialization)
{
    Buffer buffer;
    
    // Test data
    std::vector<float> testData = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dataSize = testData.size() * sizeof(float);
    
    // Initialize buffer as a general buffer
    bool result = buffer.initialize(
        device->getDevice(), 
        testData.data(), 
        dataSize,
        nvrhi::ResourceStates::ShaderResource,
        false,  // not UAV
        false,  // not constant buffer
        "TestBuffer"
    );
    
    EXPECT_TRUE(result);
    EXPECT_NE(buffer.getHandle(), nullptr);
    EXPECT_EQ(buffer.getSize(), dataSize);
}

TEST_F(BufferTest, ConstantBufferInitialization)
{
    Buffer buffer;
    
    // Test with constant buffer data
    struct TestCB
    {
        uint32_t width;
        uint32_t height;
        float value;
        float padding;
    } testData = {1920, 1080, 1.5f, 0.0f};
    
    bool result = buffer.initialize(
        device->getDevice(),
        &testData,
        sizeof(TestCB),
        nvrhi::ResourceStates::ConstantBuffer,
        false,  // not UAV
        true,   // is constant buffer
        "TestConstantBuffer"
    );
    
    EXPECT_TRUE(result);
    EXPECT_NE(buffer.getHandle(), nullptr);
    EXPECT_EQ(buffer.getSize(), sizeof(TestCB));
}

TEST_F(BufferTest, UAVBufferInitialization)
{
    Buffer buffer;
    
    // Test with UAV buffer
    std::vector<uint32_t> testData(1024, 42);  // Fill with value 42
    size_t dataSize = testData.size() * sizeof(uint32_t);
    
    bool result = buffer.initialize(
        device->getDevice(),
        testData.data(),
        dataSize,
        nvrhi::ResourceStates::UnorderedAccess,
        true,   // is UAV
        false,  // not constant buffer
        "TestUAVBuffer"
    );
    
    EXPECT_TRUE(result);
    EXPECT_NE(buffer.getHandle(), nullptr);
    EXPECT_EQ(buffer.getSize(), dataSize);
}

TEST_F(BufferTest, InitializationWithoutData)
{
    Buffer buffer;
    
    // Initialize buffer without initial data
    size_t bufferSize = 1024;
    bool result = buffer.initialize(
        device->getDevice(),
        nullptr,  // no initial data
        bufferSize,
        nvrhi::ResourceStates::ShaderResource,
        false,
        false,
        "EmptyBuffer"
    );
    
    EXPECT_TRUE(result);
    EXPECT_NE(buffer.getHandle(), nullptr);
    EXPECT_EQ(buffer.getSize(), bufferSize);
}

TEST_F(BufferTest, DataUpdate)
{
    Buffer buffer;
    
    // Initialize with initial data
    std::vector<float> initialData = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dataSize = initialData.size() * sizeof(float);
    
    bool result = buffer.initialize(
        device->getDevice(),
        initialData.data(),
        dataSize,
        nvrhi::ResourceStates::ShaderResource,
        false,
        false,
        "UpdateTestBuffer"
    );
    EXPECT_TRUE(result);
    
    // Update with new data
    std::vector<float> newData = {5.0f, 6.0f, 7.0f, 8.0f};
    buffer.updateData(device->getDevice(), newData.data(), dataSize);
    
    // The update should succeed (no exceptions or crashes)
    EXPECT_NE(buffer.getHandle(), nullptr);
}

TEST_F(BufferTest, ReadbackFunctionality)
{
    Buffer buffer;
    
    // Test data that we can verify after readback
    std::vector<uint32_t> testData = {0x12345678, 0x9ABCDEF0, 0x11223344, 0x55667788};
    size_t dataSize = testData.size() * sizeof(uint32_t);
    
    // Initialize buffer with copy source capability
    bool result = buffer.initialize(
        device->getDevice(),
        testData.data(),
        dataSize,
        nvrhi::ResourceStates::CopySource,  // Allow copy operations
        false,
        false,
        "ReadbackTestBuffer"
    );
    EXPECT_TRUE(result);
    
    // Perform readback
    auto commandList = device->getDevice()->createCommandList();
    std::vector<uint8_t> readbackData = buffer.readback(device->getDevice(), commandList);
    
    // Verify the readback data
    EXPECT_EQ(readbackData.size(), dataSize);
    
    // Compare the data
    const uint32_t* readbackUints = reinterpret_cast<const uint32_t*>(readbackData.data());
    for (size_t i = 0; i < testData.size(); ++i)
        EXPECT_EQ(readbackUints[i], testData[i]) << "Mismatch at index " << i;
}

TEST_F(BufferTest, InvalidOperations)
{
    Buffer buffer;
    
    // Test invalid initialization (zero size)
    bool result = buffer.initialize(
        device->getDevice(),
        nullptr,
        0,  // zero size should be invalid
        nvrhi::ResourceStates::ShaderResource,
        false,
        false,
        "InvalidBuffer"
    );
    
    EXPECT_EQ(buffer.getSize(), 0);
}

TEST_F(BufferTest, LargeBufferAllocation)
{
    Buffer buffer;
    
    // Test with a larger buffer (1MB)
    const size_t largeSize = 1024 * 1024;
    
    bool result = buffer.initialize(
        device->getDevice(),
        nullptr,  // no initial data for large buffer
        largeSize,
        nvrhi::ResourceStates::ShaderResource,
        false,
        false,
        "LargeBuffer"
    );
    
    EXPECT_TRUE(result);
    EXPECT_NE(buffer.getHandle(), nullptr);
    EXPECT_EQ(buffer.getSize(), largeSize);
}
