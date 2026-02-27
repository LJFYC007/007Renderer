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

// Payload: defines what we pass between shaders
struct Payload
{
    float3 color;
    uint depth;
    TinyUniformSampleGenerator sampleGen;
};

[shader("raygeneration")]
void rayGenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    if (launchID.x >= gWidth || launchID.y >= gHeight)
        return;

    Ray ray = gCamera.computeRayPinhole(launchID, gCamera.data.enableJitter);
    Payload payload;
    payload.color = float3(0, 0, 0);
    payload.depth = 1;
    payload.sampleGen = TinyUniformSampleGenerator(launchID, frameCount);

    TraceRay(gScene.rtAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray.toRayDesc(), payload);

    result[launchID] = float4(payload.color, 1.0);
}

[shader("miss")]
void missMain(inout Payload payload)
{
    // Sky color as environment lighting
    payload.color = float3(gColor);
}

[shader("closesthit")]
void closestHitMain(inout Payload payload, BuiltInTriangleIntersectionAttributes attribs)
{
    if (payload.depth >= maxDepth)
    {
        payload.color = float3(0.f);
        return;
    }

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

    ShadingData sd;
    sd.normal = hitNormal;
    sd.position = computeRayOrigin(hitPoint, hitNormal);
    sd.uv = w * uv0 + u * uv1 + v * uv2;
    sd.V = normalize(WorldRayOrigin() - hitPoint);
    sd.T = tangent;
    sd.B = bitangent;
    sd.frontFacing = frontFacing;

    uint meshIndex = gScene.triangleToMesh[primitiveIndex];
    uint materialIndex = gScene.meshes[meshIndex].materialIndex;
    GLTFMaterial material = gScene.materials[materialIndex];
    // Or use any material implementing IMaterial:
    // LambertianMaterial material = LambertianMaterial(float3(1.0f, 0.5f, 0.5f));

    // Emissive contribution
    float3 emissive = material.getEmissive(sd.uv);
    if (any(emissive > 0.f))
    {
        payload.color = emissive;
        return;
    }

    // Sample BSDF using unified interface
    BSDFSample sample = material.scatter(sd, payload.sampleGen);
    if (sample.pdf <= 0.0f)
        return;
    Ray bounceRay = Ray(sd.position, sample.wo);

    // Trace bounce ray
    Payload bouncePayload;
    bouncePayload.color = float3(0, 0, 0);
    bouncePayload.depth = payload.depth + 1;
    bouncePayload.sampleGen = payload.sampleGen;

    TraceRay(gScene.rtAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, bounceRay.toRayDesc(), bouncePayload);

    payload.color = sample.weight * bouncePayload.color;
}