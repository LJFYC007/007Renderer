#pragma once
#include <cstdint>
#include "Utils/Math/Math.h"

class TinyUniformSampleGenerator
{
public:
    TinyUniformSampleGenerator(uint32_t seed);
    TinyUniformSampleGenerator(uint2 pixel, uint32_t frameCount);

    uint32_t next();
    float nextFloat();
    float2 nextFloat2();

private:
    static uint32_t interleave32Bit(uint2 v);
    static uint2 blockCipherTEA(uint32_t v, uint32_t key);

    uint32_t mState;
};
