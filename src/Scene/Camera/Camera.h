#pragma once
#include <cstdint>

#include "Utils/Sampling/SampleGenerator.h"
#include "Utils/Math/Math.h"
#include "CameraData.slang"

class Camera
{
public:
    Camera(const float3& posW, const float3& target, float fovY, uint32_t width = 1920, uint32_t height = 1080);

    const CameraData& getCameraData() const { return mData; }
    CameraData& getCameraData() { return mData; }

    uint32_t getWidth() const { return mData.frameWidth; }
    uint32_t getHeight() const { return mData.frameHeight; }

    void setWidth(const uint32_t width)
    {
        mData.frameWidth = width;
        dirty = true;
    }

    void setHeight(const uint32_t height)
    {
        mData.frameHeight = height;
        dirty = true;
    }

    void renderUI();

    void handleInput();

    void calculateCameraParameters();

    bool dirty;

private:
    CameraData mData;
    float3 mDefaultUp = float3(0.f, 0.f, 1.f);
    bool mFirstMouseInput = true;
    TinyUniformSampleGenerator mSampleGenerator;
};