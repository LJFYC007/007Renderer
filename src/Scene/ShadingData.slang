import Utils.Math.Ray;

struct ShadingData
{
    float3 posW; // Exact hit position (no ray-origin offset)
    float3 V;    // View direction (toward camera)
    float2 uv;

    // Immutable geometric references — never modified after prepareShadingData
    float3 faceN;    // Face normal, never flipped
    float4 tangentW; // .xyz = tangent direction, .w = handedness. Never mutated.
    bool frontFacing;

    float3 getOrientedFaceNormal() { return frontFacing ? faceN : -faceN; }

    // Mutable shading frame — set by prepareShadingData, refined by prepareShadingFrame
    float3 T;
    float3 B;
    float3 N;

    uint materialID;

    float3 toLocal(float3 w) { return float3(dot(w, T), dot(w, B), dot(w, N)); }
    float3 toWorld(float3 w) { return w.x * T + w.y * B + w.z * N; }

    // Rebuild the mutable T/B/N frame from a new shading normal while keeping
    // tangentW as the geometric reference (Gram-Schmidt against N).
    [mutating]
    void buildTBN(float3 shadingN)
    {
        this.N = shadingN;
        this.T = normalize(tangentW.xyz - dot(tangentW.xyz, shadingN) * shadingN);
        this.B = tangentW.w * cross(shadingN, this.T);
    }
};
