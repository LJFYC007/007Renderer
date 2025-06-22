#pragma once

#ifdef __cplusplus
#include <glm/glm.hpp>
using float3 = glm::vec3;
#endif

struct CameraData
{
    float3 posW;
    float focalLength; 

    float aspectRatio;
    float3 _padding0;

    float3 cameraU;
    float _padding1;

    float3 cameraV;
    float _padding2;

    float3 pixelCenter;
    float _padding3;
};
