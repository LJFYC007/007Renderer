Texture2D<float4> input;
RWTexture2D<float4> output;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint gWidth, gHeight;
    input.GetDimensions(gWidth, gHeight);

    uint2 id = dispatchThreadID.xy;
    if (id.x >= gWidth || id.y >= gHeight)
        return;
    output[id] = float4(pow(input[id].rgb, 1.0 / 2.2), 1.f);
}
