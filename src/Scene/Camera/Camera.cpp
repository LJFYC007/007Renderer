#include "Camera.h"
#include "Utils/GUI.h"

#include <glm/ext.hpp>
#include <glm/gtc/constants.hpp>

Camera::Camera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, float fovY)
{
    mData.frameWidth = width;
    mData.frameHeight = height;
    mData.posW = posW;
    mData.forward = glm::normalize(target - posW);
    mData.target = target;
    mData.up = float3(0.0f, 1.0f, 0.0f); // Fixed up vector for first-person camera
    mData.fovY = fovY;
    mData.moveSpeed = 1.0f; // Default move speed in units per second

    // Initialize yaw and pitch from initial forward direction
    float3 initialForward = glm::normalize(target - posW);
    mYaw = atan2(initialForward.z, initialForward.x);
    mPitch = asin(initialForward.y);
    dirty = true;
}

void Camera::renderUI()
{
    GUI::Text("Camera Parameters");

    dirty |= GUI::DragFloat3("Position", &mData.posW.x, 0.1f, -100.0f, 100.0f);
    dirty |= GUI::DragFloat3("Target", &mData.target.x, 0.1f, -100.0f, 100.0f);
    dirty |= GUI::DragFloat3("Up Vector", &mData.up.x, 0.01f, -1.0f, 1.0f);
    dirty |= GUI::SliderFloat("FOV Y (radians)", &mData.fovY, 0.17f, 2.97f, "%.3f");

    GUI::SliderFloat("Move Speed", &mData.moveSpeed, 0.1f, 10.0f, "%.1f");
    GUI::Text("FOV Y (degrees): %.1f", mData.fovY * 180.0f / 3.14159265f);
    GUI::Text("Aspect Ratio: %.3f", mData.aspectRatio);
    GUI::Text("Focal Length: %.3f", mData.focalLength);
}

void Camera::handleInput()
{
    float3 movement(0.f);
    if (GUI::IsKeyDown(ImGuiKey_W))
        movement += mData.forward, dirty = true;
    if (GUI::IsKeyDown(ImGuiKey_S))
        movement -= mData.forward, dirty = true;
    if (GUI::IsKeyDown(ImGuiKey_A))
        movement -= mData.right, dirty = true;
    if (GUI::IsKeyDown(ImGuiKey_D))
        movement += mData.right, dirty = true;
    if (GUI::IsKeyDown(ImGuiKey_Q))
        movement -= mData.up, dirty = true;
    if (GUI::IsKeyDown(ImGuiKey_E))
        movement += mData.up, dirty = true;

    // Frame-rate independent movement using delta time
    float deltaTime = GUI::GetIO().DeltaTime;
    movement *= mData.moveSpeed * deltaTime;
    mData.posW += movement;

    // Only handle mouse input if ImGui is not using it
    if (GUI::IsMouseDown(0) && !GUI::GetIO().WantCaptureMouse) // Left mouse button
    {
        // Use ImGui's smoothed mouse delta directly instead of calculating our own
        ImVec2 mouseDelta = GUI::GetIO().MouseDelta;
        float2 delta(mouseDelta.x, mouseDelta.y);

        if (mFirstMouseInput)
        {
            // Reset delta on first frame to prevent large jumps
            delta = float2(0.0f, 0.0f);
            mFirstMouseInput = false;
        }

        // Apply delta time scaling for frame-rate independent mouse sensitivity
        float deltaTime = GUI::GetIO().DeltaTime;
        float sensitivity = 0.002f;

        if (glm::length(delta) > 0.001f)
        {
            // Update yaw (horizontal rotation) and pitch (vertical rotation)
            mYaw += delta.x * sensitivity;
            mPitch -= delta.y * sensitivity; // Invert Y for natural feel

            // Clamp pitch to prevent camera flipping
            const float maxPitch = glm::radians(89.0f);
            mPitch = glm::clamp(mPitch, -maxPitch, maxPitch);

            // Calculate new forward direction from yaw and pitch
            float3 newForward;
            newForward.x = cos(mYaw) * cos(mPitch);
            newForward.y = sin(mPitch);
            newForward.z = sin(mYaw) * cos(mPitch);
            mData.forward = glm::normalize(newForward);

            // Update target to reflect new direction
            mData.target = mData.posW + mData.forward;
            dirty = true;
        }
    }
    else
        mFirstMouseInput = true;
}

void Camera::calculateCameraParameters()
{
    if (dirty == false)
        return;
    // For first-person camera, forward is already calculated from yaw/pitch
    // Keep world up vector fixed to maintain stability
    mData.right = glm::normalize(glm::cross(mData.forward, mData.up));

    // Calculate viewport dimensions based on FOV
    mData.aspectRatio = static_cast<float>(mData.frameWidth) / static_cast<float>(mData.frameHeight);
    float viewportHeight = 2.0f;
    float viewportWidth = viewportHeight * mData.aspectRatio;
    mData.focalLength = 1.0f / glm::tan(mData.fovY * 0.5f);

    // Calculate camera U and V vectors (pixel step vectors)
    mData.cameraU = mData.right * (viewportWidth / mData.frameWidth);
    mData.cameraV = -mData.up * (viewportHeight / mData.frameHeight);

    // Calculate the center of the first pixel
    float3 viewportCenter = mData.posW + mData.forward * mData.focalLength;
    float3 viewportCorner = viewportCenter - 0.5f * viewportWidth * mData.right + 0.5f * viewportHeight * mData.up;
    mData.pixel00 = viewportCorner + 0.5f * (mData.cameraU + mData.cameraV);

    dirty = false;
}