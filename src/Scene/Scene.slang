import Scene.Material.GLTFMaterial;

struct Vertex
{
    float3 position;
    float2 texCoord;
    float3 normal;
};

struct MeshDesc
{
    uint indexOffset;
    uint indexCount;
};

struct InstanceData
{
    float4 row0;
    float4 row1;
    float4 row2;
    uint meshID;
    uint materialID;
    uint _padding0;
    uint _padding1;

    float3x4 getLocalToWorld() { return float3x4(row0, row1, row2); }
};

struct EmissiveTriangle
{
    uint instanceID;
    uint localTriangleIndex;
    float area;
    float cdfUpper;
};

struct Scene
{
    RaytracingAccelerationStructure rtAccel;
    StructuredBuffer<Vertex> vertices;
    StructuredBuffer<uint> indices;
    StructuredBuffer<GLTFMaterial> materials;
    StructuredBuffer<MeshDesc> meshes;
    StructuredBuffer<InstanceData> instances;
    StructuredBuffer<EmissiveTriangle> emissiveTriangles;
};

ParameterBlock<Scene> gScene;
