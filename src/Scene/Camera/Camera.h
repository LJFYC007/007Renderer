#include "CameraData.slang"

#include <glm/glm.hpp>
#include <cstdint>

class Camera
{
public:
    Camera(const uint32_t width, const uint32_t height, const glm::vec3& posW, const glm::vec3& target, const glm::vec3& up, float fovY)
    {
        setCamera(width, height, posW, target, up, fovY);
    }

    const CameraData& getCameraData() const { return mData; }
    CameraData& getCameraData() { return mData; }

    void setCamera(const uint32_t width, const uint32_t height, const glm::vec3& posW, const glm::vec3& target, const glm::vec3& up, float fovY);

    void renderUI();

private:
    void calculateCameraParameters();

    CameraData mData;
};