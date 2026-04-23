Texture2D<float4> source;
Texture2D<float4> reference;
RWTexture2D<float4> output;

static const uint kMetricMAE = 0;
static const uint kMetricRelMSE = 1;
static const float kRelMSEEps = 1e-2f; // per-channel denominator epsilon; prevents dark-pixel blowup

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    uint gSelectedOutput;
    uint gReferenceMode; // 0 = Texture, 1 = Constant
    float3 gConstantColor;
    uint gMetric;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint2 id = dispatchThreadID.xy;
    if (id.x >= gWidth || id.y >= gHeight)
        return;

    float4 ref = (gReferenceMode == 1) ? float4(gConstantColor, 1.f) : reference[id];

    float4 result;
    if (gSelectedOutput == 0) // Source
        result = source[id];
    else if (gSelectedOutput == 1) // Reference
        result = ref;
    else // Difference (default)
    {
        float3 diff = ref.xyz - source[id].xyz;
        float3 value = (gMetric == kMetricRelMSE) ? (diff * diff) / (ref.xyz * ref.xyz + kRelMSEEps) : abs(diff);
        result = float4(value, 1.f);
    }

    output[id] = result;
}
