import Scene.Scene;
import Utils.Math.MatrixUtils;

struct VertexData
{
    float3 posW; // Interpolated world-space position
    float2 uv;
    float3 normalW;     // Interpolated vertex normal (NOT flipped)
    float4 tangentW;    // .xyz = tangent from UV derivatives, .w = handedness sign (+1 or -1)
    float3 faceNormalW; // Flat face normal: normalize(cross(e1, e2))
    uint materialID;
};

VertexData interpolateVertices(Vertex v0, Vertex v1, Vertex v2, float2 barycentrics, float3x4 M, uint materialID)
{
    VertexData vd;
    float u = barycentrics.x;
    float v = barycentrics.y;
    float w = 1.0f - u - v;

    float3 p0w = transformPoint(M, v0.position);
    float3 p1w = transformPoint(M, v1.position);
    float3 p2w = transformPoint(M, v2.position);
    float3 e1 = p1w - p0w;
    float3 e2 = p2w - p0w;

    vd.posW = w * p0w + u * p1w + v * p2w;
    vd.uv = w * v0.texCoord + u * v1.texCoord + v * v2.texCoord;
    vd.faceNormalW = normalize(cross(e1, e2));

    float3 nLocal = w * v0.normal + u * v1.normal + v * v2.normal;
    float mirrorSign;
    vd.normalW = transformNormal(M, nLocal, mirrorSign);

    float2 duv1 = v1.texCoord - v0.texCoord;
    float2 duv2 = v2.texCoord - v0.texCoord;
    float det = duv1.x * duv2.y - duv1.y * duv2.x;

    float3 tangentW;
    float handedness;
    if (abs(det) > 1e-6f)
    {
        // World-space edges collapse mul(M, tangentLocal) into one divide by linearity.
        tangentW = normalize((e1 * duv2.y - e2 * duv1.y) / det);
        // UV parity alone is insufficient: a mirrored M flips TBN parity independently of UV.
        handedness = (det > 0.0f ? 1.0f : -1.0f) * mirrorSign;
    }
    else
    {
        float3 up = abs(vd.faceNormalW.y) < 0.9f ? float3(0, 1, 0) : float3(1, 0, 0);
        tangentW = normalize(cross(up, vd.faceNormalW));
        // Inherit mirror parity so the fallback frame stays consistent with the UV-derived branch on mirrored instances.
        handedness = mirrorSign;
    }
    vd.tangentW = float4(tangentW, handedness);
    vd.materialID = materialID;
    return vd;
}

// `localToWorld` is written unconditionally so callers can either use it (explicit variant)
// or pass a discard (hit variant, where ObjectToWorld3x4() comes from hardware and the write is DCE'd).
void loadTriangleVertices(
    uint instanceID,
    uint localPrimitiveIndex,
    out Vertex v0,
    out Vertex v1,
    out Vertex v2,
    out uint materialID,
    out float3x4 localToWorld
)
{
    InstanceData inst = gScene.instances[instanceID];
    MeshDesc mesh = gScene.meshes[inst.meshID];

    uint triStart = mesh.indexOffset + localPrimitiveIndex * 3;
    v0 = gScene.vertices[gScene.indices[triStart + 0]];
    v1 = gScene.vertices[gScene.indices[triStart + 1]];
    v2 = gScene.vertices[gScene.indices[triStart + 2]];
    materialID = inst.materialID;
    localToWorld = inst.getLocalToWorld();
}

// Explicit-instance variant: used outside hit shaders (e.g., LightSampler picks
// a random emissive triangle whose instance is unrelated to the current hit).
VertexData getVertexDataForInstance(uint instanceID, uint localPrimitiveIndex, float2 barycentrics)
{
    Vertex v0, v1, v2;
    uint materialID;
    float3x4 localToWorld;
    loadTriangleVertices(instanceID, localPrimitiveIndex, v0, v1, v2, materialID, localToWorld);
    return interpolateVertices(v0, v1, v2, barycentrics, localToWorld, materialID);
}

// Hit-context variant: PrimitiveIndex() is BLAS-local (per-mesh) since each mesh has its own BLAS.
VertexData getVertexData(uint primitiveIndex, float2 barycentrics)
{
    Vertex v0, v1, v2;
    uint materialID;
    float3x4 unusedLocalToWorld;
    loadTriangleVertices(InstanceID(), primitiveIndex, v0, v1, v2, materialID, unusedLocalToWorld);
    return interpolateVertices(v0, v1, v2, barycentrics, ObjectToWorld3x4(), materialID);
}
