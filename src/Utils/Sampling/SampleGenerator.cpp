#include "SampleGenerator.h"

uint32_t TinyUniformSampleGenerator::interleave32Bit(uint2 v)
{
    // Interleave two 16-bit integers into a 32-bit integer (Morton code / Z-order)
    uint32_t x = v.x & 0xFFFF;
    uint32_t y = v.y & 0xFFFF;

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

uint2 TinyUniformSampleGenerator::blockCipherTEA(uint32_t v, uint32_t key)
{
    // Tiny Encryption Algorithm (TEA) used as hash function
    uint32_t v0 = v;
    uint32_t v1 = 0;
    uint32_t sum = 0;
    uint32_t delta = 0x9E3779B9;

    for (uint32_t i = 0; i < 4; i++) // 4-round TEA block cipher
    {
        sum += delta;
        v0 += ((v1 << 4) + key) ^ (v1 + sum) ^ ((v1 >> 5) + key);
        v1 += ((v0 << 4) + key) ^ (v0 + sum) ^ ((v0 >> 5) + key);
    }

    return uint2(v0, v1);
}

TinyUniformSampleGenerator::TinyUniformSampleGenerator(uint32_t seed)
    : mState(seed)
{
}

TinyUniformSampleGenerator::TinyUniformSampleGenerator(uint2 pixel, uint32_t frameCount)
{
    uint32_t seed = blockCipherTEA(interleave32Bit(pixel), frameCount).x;
    mState = seed;
}

uint32_t TinyUniformSampleGenerator::next()
{
    const uint32_t A = 1664525u;
    const uint32_t C = 1013904223u;
    mState = (A * mState + C);
    return mState;
}

float TinyUniformSampleGenerator::nextFloat()
{
    // Use upper 24 bits and divide by 2^24 to get a number u in [0,1).
    // In floating-point precision this also ensures that 1.0-u != 0.0.
    uint32_t x = next() >> 8; 
    return x * (1.0f / 16777216.0f); 
}

float2 TinyUniformSampleGenerator::nextFloat2()
{
    return float2(nextFloat(), nextFloat());
}
