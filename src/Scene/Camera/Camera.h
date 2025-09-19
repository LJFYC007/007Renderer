#pragma once
#include <cstdint>

#include "Utils/Sampling/SampleGenerator.h"
#include "Utils/Math/Math.h"
#include "CameraData.slang"

class Camera
{
public:
    Camera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, float fovY);

    const CameraData& getCameraData() const { return mData; }
    CameraData& getCameraData() { return mData; }

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
    bool mFirstMouseInput = true;

    // First person camera rotation angles
    float mYaw = 0.0f;   // Horizontal rotation (around Y axis)
    float mPitch = 0.0f; // Vertical rotation (around X axis)

    TinyUniformSampleGenerator mSampleGenerator;
};