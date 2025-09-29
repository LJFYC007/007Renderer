#include <glm/ext.hpp>
#include <glm/gtc/constants.hpp>

#include "Camera.h"
#include "Utils/GUI.h"

Camera::Camera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, float fovY) : mSampleGenerator(233)
{
    mData.frameWidth = width;
    mData.frameHeight = height;
    mData.posW = posW;
    mData.forward = glm::normalize(target - posW);
    mData.target = target;
    mData.up = float3(0.0f, 1.0f, 0.0f);
    mData.fovY = fovY;
    mData.moveSpeed = 1.0f; // Default move speed in units per second
    mData.enableJitter = true;
    dirty = true;
}

void Camera::renderUI()
{
    dirty |= GUI::DragFloat3("Position", &mData.posW.x, 0.1f, -100.0f, 100.0f);
    dirty |= GUI::DragFloat3("Target", &mData.target.x, 0.1f, -100.0f, 100.0f);
    dirty |= GUI::DragFloat3("Up Vector", &mData.up.x, 0.01f, -1.0f, 1.0f);
    dirty |= GUI::SliderFloat("FOV Y", &mData.fovY, 0.17f, 2.97f, "%.3f"); // radians

    GUI::SliderFloat("Move Speed", &mData.moveSpeed, 0.1f, 10.0f, "%.1f");
    GUI::Checkbox("Enable Jitter", &mData.enableJitter);
}

void Camera::handleInput()
{
    float3 movement(0.f);
    float3 viewDir = normalize(mData.target - mData.posW);
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
    mData.target = mData.posW + viewDir;

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
            float3 camUp = float3(0.f, 1.f, 0.f);
            float3 sideway = cross(viewDir, normalize(camUp));
            float2 rotation = -delta * sensitivity;

            glm::quat qy = glm::angleAxis(rotation.y, sideway);
            viewDir = qy * viewDir;
            camUp = qy * camUp;
            glm::quat qx = glm::angleAxis(rotation.x, camUp);
            viewDir = qx * viewDir;

            mData.target = mData.posW + viewDir;
            mData.up = camUp;
            dirty = true;
        }
    }
    else
        mFirstMouseInput = true;
}

void Camera::calculateCameraParameters()
{
    if (mData.enableJitter)
        mData.jitter = mSampleGenerator.nextFloat2() - float2(0.5f, 0.5f);

    if (dirty == false)
        return;

    // Calculate viewport dimensions based on FOV
    mData.aspectRatio = static_cast<float>(mData.frameWidth) / static_cast<float>(mData.frameHeight);
    float viewportHeight = 2.0f;
    float viewportWidth = viewportHeight * mData.aspectRatio;
    mData.focalLength = 1.0f / glm::tan(mData.fovY * 0.5f);

    mData.forward = glm::normalize(mData.target - mData.posW);
    mData.right = glm::normalize(glm::cross(mData.forward, mData.up));
    mData.up = glm::normalize(glm::cross(mData.right, mData.forward));

    // Calculate camera U and V vectors (pixel step vectors)
    mData.cameraU = mData.right * (viewportWidth / mData.frameWidth);
    mData.cameraV = -mData.up * (viewportHeight / mData.frameHeight);

    // Calculate the center of the first pixel
    float3 viewportCenter = mData.posW + mData.forward * mData.focalLength;
    float3 viewportCorner = viewportCenter - 0.5f * viewportWidth * mData.right + 0.5f * viewportHeight * mData.up;
    mData.pixel00 = viewportCorner + 0.5f * (mData.cameraU + mData.cameraV);

    dirty = false;
}