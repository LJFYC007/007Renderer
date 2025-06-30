#include "Utils/Math/Math.h"
#include "CameraData.slang"

#include <cstdint>

class Camera
{
public:
    Camera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, float fovY)
    {
        setCamera(width, height, posW, target, fovY);
    }

    const CameraData& getCameraData() const { return mData; }
    CameraData& getCameraData() { return mData; }

    void setCamera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, float fovY);

    void renderUI();

    void handleInput();

private:
    void calculateCameraParameters();

    CameraData mData;
    bool mFirstMouseInput = true;

    // First person camera rotation angles
    float mYaw = 0.0f;   // Horizontal rotation (around Y axis)
    float mPitch = 0.0f; // Vertical rotation (around X axis)
};