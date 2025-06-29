#include "Utils/Math/MathConstants.slangh"

struct Ray
{
    float3 origin;
    float tMin;
    float3 dir;
    float tMax;

    __init(float3 origin, float3 dir, float tMin = 0.f, float tMax = FLT_MAX)
    {
        this.origin = origin;
        this.dir = dir;
        this.tMin = tMin;
        this.tMax = tMax;
    }

    float3 eval(float t) { return origin + t * dir; }

    RayDesc toRayDesc() { return RayDesc(origin, tMin, dir, tMax); }
}