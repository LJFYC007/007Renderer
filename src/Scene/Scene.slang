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
    Texture2D textures[2];                 // Array of textures (adjust size as needed)
    SamplerState textureSampler;
};

ParameterBlock<Scene> gScene;