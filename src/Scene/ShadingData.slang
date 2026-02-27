struct ShadingData
{
    float3 position;
    float3 normal;
    float2 uv;
    float3 V;         // view direction: from shading point toward camera
    float3 T;         // Tangent
    float3 B;         // Bitangent (handedness-aware: cross(T,B)==normal)
    bool frontFacing; // true when ray hit the geometrically outward face

    float3 toLocal(float3 w) { return float3(dot(w, T), dot(w, B), dot(w, normal)); }

    float3 toWorld(float3 w) { return w.x * T + w.y * B + w.z * normal; }
};