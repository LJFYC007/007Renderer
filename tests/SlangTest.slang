struct Constants
{
    float scalarValue;
    int integerValue;
    bool flagValue;
    float _padding; // Align the following float3 to the next 16-byte boundary
    float3 vectorValue;
};

StructuredBuffer<float> gInputBuffer;
Texture2D<float4> gInputTexture;
ConstantBuffer<Constants> gConstants;
RWStructuredBuffer<float4> gOutputBuffer;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    float bufferSample = gInputBuffer[index];
    float4 textureSample = gInputTexture.Load(int3(index, 0, 0));
    float accumulator =
        gConstants.scalarValue + float(gConstants.integerValue) + (gConstants.flagValue ? 1.0f : 0.0f) + gConstants.vectorValue.z;

    gOutputBuffer[index] = float4(bufferSample, textureSample.y, accumulator, gConstants.vectorValue.y);
}
