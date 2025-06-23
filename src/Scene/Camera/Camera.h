#include "Utils/Math/Math.h"
#include "CameraData.slang"

#include <cstdint>

class Camera
{
public:
    Camera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, const float3& up, float fovY)
    {
        setCamera(width, height, posW, target, up, fovY);
    }

    const CameraData& getCameraData() const { return mData; }
    CameraData& getCameraData() { return mData; }

    void setCamera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, const float3& up, float fovY);

    void renderUI();

    void handleInput();

private:
    void calculateCameraParameters();

    CameraData mData;
    float2 mLastMousePos = float2(0.0f, 0.0f);
    bool mFirstMouseInput = true;
};