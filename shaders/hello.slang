StructuredBuffer<float> buffer0;
StructuredBuffer<float> buffer1;
RWStructuredBuffer<float> result;

float plus(float a, float b)
{
    return a + b;
}

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint idx = dispatchThreadID.x;
    result[idx] = plus(buffer0[idx], buffer1[idx]);
}
