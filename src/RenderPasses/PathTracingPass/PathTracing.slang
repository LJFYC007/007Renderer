import Utils.Math.Ray;
import Utils.Sampling.SampleGenerator;
import Scene.Camera.Camera;
import Scene.Scene;
import Scene.Material.GLTFMaterial;
import Scene.ShadingData;

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    uint maxDepth;
    uint frameCount;
    float gColor;
};

ConstantBuffer<Camera> gCamera;
RWTexture2D<float4> result;

/**
 * Payload for scatter ray
 */
struct ScatterRayData
{
    float3 radiance;  ///< Accumulated outgoing radiance from path.
    bool terminated;  ///< Set to true when path is terminated.
    float3 thp;       ///< Current path throughput. This is updated at each path vertex.
    uint pathLength;  ///< Path length in number of path segments (0 at origin, 1 at first secondary hit, etc.). Max 2^31.
    float3 origin;    ///< Next path segment origin.
    float3 direction; ///< Next path segment direction.

    TinyUniformSampleGenerator sg; ///< Per-ray state for the sample generator (up to 16B).

    __init(TinyUniformSampleGenerator sg)
    {
        this.terminated = false;
        this.pathLength = 0;
        this.radiance = float3(0, 0, 0);
        this.thp = float3(1, 1, 1);
        this.origin = float3(0, 0, 0);
        this.direction = float3(0, 0, 0);
        this.sg = sg;
    }
};

[shader("raygeneration")]
void rayGenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    if (launchID.x >= gWidth || launchID.y >= gHeight)
        return;

    ScatterRayData sd = ScatterRayData(TinyUniformSampleGenerator(launchID, frameCount));
    Ray ray = gCamera.computeRayPinhole(launchID, gCamera.data.enableJitter);

    for (uint bounce = 0; bounce <= maxDepth; bounce++)
    {
        TraceRay(gScene.rtAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray.toRayDesc(), sd);
        if (sd.terminated)
            break;
        ray = Ray(sd.origin, sd.direction);
        sd.pathLength++;
    }

    result[launchID] = float4(sd.radiance, 1.0);
}

[shader("miss")]
void missMain(inout ScatterRayData sd)
{
    sd.radiance += sd.thp * float3(gColor);
    sd.terminated = true;
}

[shader("closesthit")]
void closestHitMain(inout ScatterRayData sd, BuiltInTriangleIntersectionAttributes attribs)
{
    // Get barycentric coordinates
    float u = attribs.barycentrics.x;
    float v = attribs.barycentrics.y;
    float w = 1.0f - u - v;

    // Get primitive index (triangle index)
    uint primitiveIndex = PrimitiveIndex();

    // Get vertex indices for this triangle
    uint baseIndex = primitiveIndex * 3;
    uint i0 = gScene.indices[baseIndex + 0];
    uint i1 = gScene.indices[baseIndex + 1];
    uint i2 = gScene.indices[baseIndex + 2];

    // Get vertex data
    float3 pos0 = gScene.vertices[i0].position;
    float3 pos1 = gScene.vertices[i1].position;
    float3 pos2 = gScene.vertices[i2].position;
    float2 uv0 = gScene.vertices[i0].texCoord;
    float2 uv1 = gScene.vertices[i1].texCoord;
    float2 uv2 = gScene.vertices[i2].texCoord;
    float3 normal0 = gScene.vertices[i0].normal;
    float3 normal1 = gScene.vertices[i1].normal;
    float3 normal2 = gScene.vertices[i2].normal;

    float3 hitNormal = normalize(w * normal0 + u * normal1 + v * normal2);
    bool frontFacing = dot(hitNormal, WorldRayDirection()) <= 0.0f;
    if (!frontFacing)
        hitNormal = -hitNormal;
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Calculate tangent space vectors
    float3 deltaPos1 = pos1 - pos0;
    float3 deltaPos2 = pos2 - pos0;
    float2 deltaUV1 = uv1 - uv0;
    float2 deltaUV2 = uv2 - uv0;

    float det = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
    float3 tangent;
    // UV handedness: +1 for standard winding, -1 for mirrored UV islands.
    // Derived from det before any normal flip so it stays correct for back faces.
    float handedness = 1.0f;

    if (abs(det) > 1e-6f)
    {
        float invDet = 1.0f / det;
        tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * invDet;
        handedness = det > 0.0f ? 1.0f : -1.0f;
    }
    else
    {
        float3 up = abs(hitNormal.y) < 0.9f ? float3(0, 1, 0) : float3(1, 0, 0);
        tangent = normalize(cross(up, hitNormal));
    }

    // Gram-Schmidt against the (possibly flipped) hitNormal, then derive bitangent
    // using handedness so that cross(T, B) == hitNormal always holds.
    tangent = normalize(tangent - dot(tangent, hitNormal) * hitNormal);
    float3 bitangent = handedness * cross(hitNormal, tangent);

    ShadingData hit;
    hit.normal = hitNormal;
    hit.position = computeRayOrigin(hitPoint, hitNormal);
    hit.uv = w * uv0 + u * uv1 + v * uv2;
    hit.V = normalize(WorldRayOrigin() - hitPoint);
    hit.T = tangent;
    hit.B = bitangent;
    hit.frontFacing = frontFacing;

    uint meshIndex = gScene.triangleToMesh[primitiveIndex];
    uint materialIndex = gScene.meshes[meshIndex].materialIndex;
    GLTFMaterial material = gScene.materials[materialIndex];

    float3 emissive = material.getEmissive(hit.uv);
    if (any(emissive > 0.f))
    {
        sd.radiance += sd.thp * emissive;
        sd.terminated = true;
        return;
    }

    BSDFSample sample = material.scatter(hit, sd.sg);
    if (sample.pdf <= 0.0f)
    {
        sd.terminated = true;
        return;
    }

    sd.thp *= sample.weight;
    sd.origin = hit.position;
    sd.direction = sample.wo;
}