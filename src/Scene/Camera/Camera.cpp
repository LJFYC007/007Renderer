#include "Camera.h"
#include "Utils/GUI.h"

void Camera::setCamera(const uint32_t width, const uint32_t height, const glm::vec3& posW, const glm::vec3& target, const glm::vec3& up, float fovY)
{
    mData.frameWidth = width;
    mData.frameHeight = height;
    mData.posW = posW;
    mData.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    mData.target = target;
    mData.up = up;
    mData.fovY = fovY;

    calculateCameraParameters();
}

void Camera::renderUI()
{
    bool dirty = false;
    GUI::Text("Camera Parameters");

    dirty |= GUI::DragFloat3("Position", &mData.posW.x, 0.1f, -100.0f, 100.0f);
    dirty |= GUI::DragFloat3("Target", &mData.target.x, 0.1f, -100.0f, 100.0f);
    dirty |= GUI::DragFloat3("Up Vector", &mData.up.x, 0.01f, -1.0f, 1.0f);
    dirty |= GUI::SliderFloat("FOV Y (radians)", &mData.fovY, 0.17f, 2.97f, "%.3f");

    GUI::Text("FOV Y (degrees): %.1f", mData.fovY * 180.0f / 3.14159265f);
    GUI::Text("Aspect Ratio: %.3f", mData.aspectRatio);
    GUI::Text("Focal Length: %.3f", mData.focalLength);

    // Recalculate camera parameters if any value was updated
    if (dirty)
        calculateCameraParameters();
}

void Camera::calculateCameraParameters()
{
    // Calculate camera basis vectors
    glm::vec3 forward = glm::normalize(mData.target - mData.posW);
    glm::vec3 right = glm::normalize(glm::cross(forward, mData.up));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // Calculate viewport dimensions based on FOV
    float viewportHeight = 2.0f;
    float viewportWidth = viewportHeight * mData.aspectRatio;
    mData.focalLength = 1.0f / glm::tan(mData.fovY * 0.5f);

    // Calculate camera U and V vectors (pixel step vectors)
    mData.cameraU = right * (viewportWidth / mData.frameWidth);
    mData.cameraV = up * (viewportHeight / mData.frameHeight);

    // Calculate the center of the first pixel
    glm::vec3 viewportCenter = mData.posW + forward * mData.focalLength;
    glm::vec3 viewportCorner = viewportCenter - 0.5f * viewportWidth * right - 0.5f * viewportHeight * up;
    mData.pixel00 = viewportCorner + 0.5f * (mData.cameraU + mData.cameraV);
}