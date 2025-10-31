import Scene.Material.GLTFMaterial;

struct Vertex
{
    float3 position;
    float2 texCoord;
    float3 normal;
};

struct MeshInfo
{
    uint materialIndex;
};

struct Scene
{
    RaytracingAccelerationStructure rtAccel;
    StructuredBuffer<Vertex> vertices;
    StructuredBuffer<uint> indices;
    StructuredBuffer<GLTFMaterial> materials;
    StructuredBuffer<MeshInfo> meshes;
    StructuredBuffer<uint> triangleToMesh; // Maps triangle index to mesh index
};

ParameterBlock<Scene> gScene;