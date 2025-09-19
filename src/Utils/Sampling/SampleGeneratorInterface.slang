interface ISampleGenerator
{
    [mutating]
    uint next();
};

float sampleNext1D<S : ISampleGenerator>(inout S sg)
{
    // Use upper 24 bits and divide by 2^24 to get a number u in [0,1).
    // In floating-point precision this also ensures that 1.0-u != 0.0.
    uint bits = sg.next();
    return (bits >> 8) * 0x1p-24;
}

float2 sampleNext2D<S : ISampleGenerator>(inout S sg)
{
    float2 sample;
    sample.x = sampleNext1D(sg);
    sample.y = sampleNext1D(sg);
    return sample;
}

float3 sampleNext3D<S : ISampleGenerator>(inout S sg)
{
    float3 sample;
    sample.x = sampleNext1D(sg);
    sample.y = sampleNext1D(sg);
    sample.z = sampleNext1D(sg);
    return sample;
}

float4 sampleNext4D<S : ISampleGenerator>(inout S sg)
{
    float4 sample;
    sample.x = sampleNext1D(sg);
    sample.y = sampleNext1D(sg);
    sample.z = sampleNext1D(sg);
    sample.w = sampleNext1D(sg);
    return sample;
}
