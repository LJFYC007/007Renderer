#include "Utils/Math/MathConstants.slangh"
__exported import Utils.Sampling.SampleGeneratorInterface;

uint interleave_32bit(uint2 v)
{
    // Interleave two 16-bit integers into a 32-bit integer (Morton code / Z-order)
    uint x = v.x & 0xFFFF;
    uint y = v.y & 0xFFFF;

    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;

    y = (y | (y << 8)) & 0x00FF00FF;
    y = (y | (y << 4)) & 0x0F0F0F0F;
    y = (y | (y << 2)) & 0x33333333;
    y = (y | (y << 1)) & 0x55555555;

    return x | (y << 1);
}

uint2 blockCipherTEA(uint v, uint key)
{
    // Tiny Encryption Algorithm (TEA) used as hash function
    uint v0 = v;
    uint v1 = 0;
    uint sum = 0;
    uint delta = 0x9E3779B9;

    for (uint i = 0; i < 4; i++) // 4-round TEA block cipher
    {
        sum += delta;
        v0 += ((v1 << 4) + key) ^ (v1 + sum) ^ ((v1 >> 5) + key);
        v1 += ((v0 << 4) + key) ^ (v0 + sum) ^ ((v0 >> 5) + key);
    }

    return uint2(v0, v1);
}

export struct TinyUniformSampleGenerator : ISampleGenerator
{
    __init(uint seed) { this.state = seed; }

    __init(uint2 pixel, uint frameCount)
    {
        uint seed = blockCipherTEA(interleave_32bit(pixel), frameCount).x;
        this.state = seed;
    }

    [mutating] uint next()
    {
        const uint A = 1664525u;
        const uint C = 1013904223u;
        this.state = (A * this.state + C);
        return this.state;
    }

    uint state;
    uint3 _pad;
};

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
