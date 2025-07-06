// Vertex structure that matches C++ side
struct Vertex
{
    float3 position;
    float2 texCoord;
    float3 normal;
};

struct Scene
{
    RaytracingAccelerationStructure rtAccel;
    StructuredBuffer<Vertex> vertices;
    StructuredBuffer<uint> indices;
};

ParameterBlock<Scene> gScene;