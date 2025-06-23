#include "CameraData.slang"

#include <glm/glm.hpp>
#include <cstdint>

class Camera
{
public:
    Camera(uint32_t width, uint32_t height)
    {
        mData.frameWidth = width;
        mData.frameHeight = height;
        mData.posW = glm::vec3(0.0f, 0.0f, 1.0f);
        mData.focalLength = 1.0f;
        mData.aspectRatio = static_cast<float>(mData.frameWidth) / static_cast<float>(mData.frameHeight);
        updateCameraVectors();
    }

    const CameraData& getCameraData() const { return mData; }

private:
    void updateCameraVectors()
    {
        float viewportHeight = 2.0f;
        float viewportWidth = viewportHeight * mData.aspectRatio;
        mData.cameraU = glm::vec3(viewportWidth / mData.frameWidth, 0.0f, 0.0f);
        mData.cameraV = glm::vec3(0.0f, viewportHeight / mData.frameHeight, 0.0f);

        glm::vec3 screenCenter = mData.posW - glm::vec3(0.0f, 0.0f, mData.focalLength);
        mData.pixelCenter = screenCenter - glm::vec3(viewportWidth / 2, viewportHeight / 2, 0.0f) + 0.5f * (mData.cameraU + mData.cameraV);
    }

    mutable CameraData mData;
};