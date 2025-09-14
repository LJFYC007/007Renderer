StructuredBuffer<float> BufferA;
StructuredBuffer<float> BufferB;
RWStructuredBuffer<float> BufferResult;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint idx = dispatchThreadID.x;
    BufferResult[idx] = BufferA[idx] + BufferB[idx];
}