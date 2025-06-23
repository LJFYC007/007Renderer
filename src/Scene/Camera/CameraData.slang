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

    float3 pixelCenter;
    float aspectRatio;

    float3 cameraU;
    uint frameWidth;

    float3 cameraV;
    uint frameHeight;
};
