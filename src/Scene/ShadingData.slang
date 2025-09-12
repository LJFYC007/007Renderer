struct ShadingData
{
    float3 position;
    float3 normal;
    float3 V; // view direction which is position - origin
    float3 T; // Tangent
    float3 B; // Bitangent

    float3 toLocal(float3 w) { return float3(dot(w, T), dot(w, B), dot(w, normal)); }

    float3 toWorld(float3 w) { return w.x * T + w.y * B + w.z * normal; }
};