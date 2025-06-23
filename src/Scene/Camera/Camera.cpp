#include "Camera.h"
#include "Utils/GUI.h"

#include <glm/ext.hpp>

void Camera::setCamera(const uint32_t width, const uint32_t height, const float3& posW, const float3& target, const float3& up, float fovY)
{
    mData.frameWidth = width;
    mData.frameHeight = height;
    mData.posW = posW;
    mData.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    mData.target = target;
    mData.up = up;
    mData.fovY = fovY;
    mData.moveSpeed = 1.0f; // Default move speed in units per second

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

    GUI::SliderFloat("Move Speed", &mData.moveSpeed, 0.1f, 10.0f, "%.1f");
    GUI::Text("FOV Y (degrees): %.1f", mData.fovY * 180.0f / 3.14159265f);
    GUI::Text("Aspect Ratio: %.3f", mData.aspectRatio);
    GUI::Text("Focal Length: %.3f", mData.focalLength);

    // Recalculate camera parameters if any value was updated
    if (dirty)
        calculateCameraParameters();
}

void Camera::handleInput()
{
    float3 movement(0.f);
    if (GUI::IsKeyDown(ImGuiKey_W))
        movement += mData.forward;
    if (GUI::IsKeyDown(ImGuiKey_S))
        movement -= mData.forward;
    if (GUI::IsKeyDown(ImGuiKey_A))
        movement -= mData.right;
    if (GUI::IsKeyDown(ImGuiKey_D))
        movement += mData.right;
    if (GUI::IsKeyDown(ImGuiKey_Q))
        movement -= mData.up;
    if (GUI::IsKeyDown(ImGuiKey_E))
        movement += mData.up;

    // Frame-rate independent movement using delta time
    float deltaTime = GUI::GetIO().DeltaTime;
    movement *= mData.moveSpeed * deltaTime;
    mData.posW += movement;
    mData.target += movement;

    // ----------------------------
    // Mouse drag for orbit rotation
    // ----------------------------

    if (GUI::IsMouseDown(0)) // Left mouse button
    {
        float2 currentMousePos(GUI::GetMousePos().x, GUI::GetMousePos().y);
        if (mFirstMouseInput)
        {
            mLastMousePos = currentMousePos;
            mFirstMouseInput = false;
        }

        float2 delta = mLastMousePos - currentMousePos;
        if (glm::length(delta) > 0.001f) // Only process if there's meaningful movement
        {
            float sensitivity = 0.005f;
            float3 offset = mData.target - mData.posW;
            float distance = glm::length(offset);
            offset = glm::normalize(offset);

            // Rotate around up vector (yaw) - horizontal mouse movement
            if (abs(delta.x) > 0.001f)
            {
                float angleYaw = delta.x * sensitivity;
                glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), angleYaw, mData.up);
                offset = glm::vec3(yawRotation * glm::vec4(offset, 1.0f));
            }

            // Rotate around right vector (pitch) - vertical mouse movement
            if (abs(delta.y) > 0.001f)
            {
                float anglePitch = -delta.y * sensitivity;
                glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), anglePitch, mData.right);
                offset = glm::vec3(pitchRotation * glm::vec4(offset, 1.0f));
            }

            mData.target = mData.posW + offset * distance;
        }
        mLastMousePos = currentMousePos;
    }
    else
        mFirstMouseInput = true; // Reset for next mouse drag session

    calculateCameraParameters();
}

void Camera::calculateCameraParameters()
{
    // Calculate camera basis vectors
    mData.forward = glm::normalize(mData.target - mData.posW);
    mData.right = glm::normalize(glm::cross(mData.forward, mData.up));
    mData.up = glm::normalize(glm::cross(mData.right, mData.forward));

    // Calculate viewport dimensions based on FOV
    float viewportHeight = 2.0f;
    float viewportWidth = viewportHeight * mData.aspectRatio;
    mData.focalLength = 1.0f / glm::tan(mData.fovY * 0.5f);

    // Calculate camera U and V vectors (pixel step vectors)
    mData.cameraU = mData.right * (viewportWidth / mData.frameWidth);
    mData.cameraV = mData.up * (viewportHeight / mData.frameHeight);

    // Calculate the center of the first pixel
    float3 viewportCenter = mData.posW + mData.forward * mData.focalLength;
    float3 viewportCorner = viewportCenter - 0.5f * viewportWidth * mData.right - 0.5f * viewportHeight * mData.up;
    mData.pixel00 = viewportCorner + 0.5f * (mData.cameraU + mData.cameraV);
}