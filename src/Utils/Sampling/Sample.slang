#include "Utils/Math/MathConstants.slangh"

float3 sampleSphere(float2 u)
{
    float z = 1.0f - 2.0f * u.x;            // z ∈ [-1, 1]
    float r = sqrt(max(0.0, 1.0f - z * z)); // radius in XY plane
    float phi = TWO_PI * u.y;               // azimuthal angle
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, z); // unit vector on the sphere
}

float3 sampleHemisphere(float3 normal, float2 u)
{
    float3 x = sampleSphere(u);
    x = dot(x, normal) < 0.0f ? -x : x; // Ensure the sample is in the same hemisphere as the normal
    return x;
}
