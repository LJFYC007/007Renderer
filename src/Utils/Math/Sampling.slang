#include "Utils/Math/MathConstants.slangh"

// Hash function for better randomness distribution
uint hash(uint x)
{
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

// Wang hash for improved distribution quality
uint wangHash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

// Xorshift random number generator (higher quality than simple LCG)
uint nextRandom(inout uint seed)
{
    seed ^= seed << 13u;
    seed ^= seed >> 17u;
    seed ^= seed << 5u;
    return seed;
}

// Generate pseudo-random float in [0, 1)
float rnd(inout uint seed)
{
    return float(nextRandom(seed)) / float(0xffffffffu);
}

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
