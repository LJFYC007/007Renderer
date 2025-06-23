#pragma once

#ifdef __cplusplus
#include <glm/glm.hpp>
using float3 = glm::vec3;
using uint = uint32_t;
#endif

struct CameraData
{
    float3 posW;
    float focalLength; 

    float3 pixel00;
    float aspectRatio;

    float3 cameraU;
    uint frameWidth;

    float3 cameraV;
    uint frameHeight;

    float3 target;
    float fovY;

    float3 up;
    float _padding0; // Padding to ensure 16-byte alignment
};
