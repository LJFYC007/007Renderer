import Scene.Scene;

struct VertexData
{
    float3 posW; // Interpolated world-space position
    float2 uv;
    float3 normalW;     // Interpolated vertex normal (NOT flipped)
    float4 tangentW;    // .xyz = tangent from UV derivatives, .w = handedness sign (+1 or -1)
    float3 faceNormalW; // Flat face normal: normalize(cross(e1, e2))
    uint materialID;
};

VertexData getVertexData(uint primitiveIndex, float2 barycentrics)
{
    VertexData vd;
    float u = barycentrics.x;
    float v = barycentrics.y;
    float w = 1.0f - u - v;

    uint baseIndex = primitiveIndex * 3;
    uint i0 = gScene.indices[baseIndex + 0];
    uint i1 = gScene.indices[baseIndex + 1];
    uint i2 = gScene.indices[baseIndex + 2];

    float3 p0 = gScene.vertices[i0].position;
    float3 p1 = gScene.vertices[i1].position;
    float3 p2 = gScene.vertices[i2].position;

    vd.posW = w * p0 + u * p1 + v * p2;
    vd.uv = w * gScene.vertices[i0].texCoord + u * gScene.vertices[i1].texCoord + v * gScene.vertices[i2].texCoord;
    vd.normalW = normalize(w * gScene.vertices[i0].normal + u * gScene.vertices[i1].normal + v * gScene.vertices[i2].normal);

    float3 e1 = p1 - p0;
    float3 e2 = p2 - p0;
    vd.faceNormalW = normalize(cross(e1, e2));

    float2 duv1 = gScene.vertices[i1].texCoord - gScene.vertices[i0].texCoord;
    float2 duv2 = gScene.vertices[i2].texCoord - gScene.vertices[i0].texCoord;
    float det = duv1.x * duv2.y - duv1.y * duv2.x;

    float3 tangent;
    float handedness = 1.0f;
    if (abs(det) > 1e-6f)
    {
        tangent = normalize((e1 * duv2.y - e2 * duv1.y) / det);
        handedness = det > 0.0f ? 1.0f : -1.0f;
    }
    else
    {
        float3 up = abs(vd.faceNormalW.y) < 0.9f ? float3(0, 1, 0) : float3(1, 0, 0);
        tangent = normalize(cross(up, vd.faceNormalW));
    }
    vd.tangentW = float4(tangent, handedness);

    uint meshIndex = gScene.triangleToMesh[primitiveIndex];
    vd.materialID = gScene.meshes[meshIndex].materialIndex;

    return vd;
}
