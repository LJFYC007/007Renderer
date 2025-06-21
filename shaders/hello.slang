StructuredBuffer<float3> buffer0;
StructuredBuffer<float3> buffer1;
RWTexture2D<float3> result;
static const uint width = 1920;
static const uint height = 1080;

float3 plus(float3 a, float3 b)
{
    return a + b;
}

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    
    if (pixelCoord.x >= width || pixelCoord.y >= height)
        return;
    uint id = pixelCoord.y * width + pixelCoord.x;
    
    float3 valueA = buffer0[id];
    float3 valueB = buffer1[id];
    result[pixelCoord] = plus(valueA, valueB);
}
