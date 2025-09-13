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

float3
computeRayOrigin(float3 pos, float3 normal)
{
    const float origin = 1.f / 16.f;
    const float fScale = 3.f / 65536.f;
    const float iScale = 3 * 256.f;

    // Per-component integer offset to bit representation of fp32 position.
    int3 iOff = int3(normal * iScale);
    float3 iPos = asfloat(asint(pos) + select(pos < 0.f, -iOff, iOff));

    // Select per-component between small fixed offset or above variable offset depending on distance to origin.
    float3 fOff = normal * fScale;
    return select(abs(pos) < origin, pos + fOff, iPos);
}