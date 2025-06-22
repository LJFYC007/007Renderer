#pragma once

#ifdef __cplusplus
#include <glm/glm.hpp>
using float3 = glm::vec3;
#endif

struct CameraData
{
    float3 posW;
    float focalLength;
    float aspectRatio = 1.7777777f;
    float3 cameraU;
    float3 cameraV;
    float3 pixelCenter;
};