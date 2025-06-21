cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    float gColor;
};

StructuredBuffer<float3> buffer0;
StructuredBuffer<float3> buffer1;
RWTexture2D<float3> result;

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;

    if (pixelCoord.x >= gWidth || pixelCoord.y >= gHeight)
        return;
    uint id = pixelCoord.y * gWidth + pixelCoord.x;
    
    float3 valueA = buffer0[id];
    float3 valueB = buffer1[id];
    // result[pixelCoord] = valueA + valueB;
    result[pixelCoord] = float3(gColor);
}
