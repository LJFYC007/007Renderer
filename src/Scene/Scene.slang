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

struct EmissiveTriangle
{
    uint triangleIndex; // Global primitive index
    float area;         // World-space triangle area
    float cdfUpper;     // Cumulative area / totalArea (upper bound of CDF bin)
    float _padding;
};

struct Scene
{
    RaytracingAccelerationStructure rtAccel;
    StructuredBuffer<Vertex> vertices;
    StructuredBuffer<uint> indices;
    StructuredBuffer<GLTFMaterial> materials;
    StructuredBuffer<MeshInfo> meshes;
    StructuredBuffer<uint> triangleToMesh;                // Maps triangle index to mesh index
    StructuredBuffer<EmissiveTriangle> emissiveTriangles; // Area-weighted CDF for light sampling
};

ParameterBlock<Scene> gScene;