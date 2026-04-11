import Utils.Math.Ray;
import Utils.Sampling.SampleGenerator;
import Scene.Camera.Camera;
import Scene.Scene;
import Scene.VertexData;
import Scene.ShadingData;
import Scene.ShadingPrep;
import Scene.Material.BSDFTypes;
import Scene.Material.GLTFMaterial;

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

struct ScatterRayData
{
    float3 radiance;
    bool terminated;
    float3 thp;
    uint pathLength;
    float3 origin;
    float3 direction;

    TinyUniformSampleGenerator sg; // Per-ray state for the sample generator (up to 16B)

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

    ScatterRayData scatterRay = ScatterRayData(TinyUniformSampleGenerator(launchID, frameCount));
    Ray ray = gCamera.computeRayPinhole(launchID, gCamera.data.enableJitter);

    for (uint bounce = 0; bounce <= maxDepth; bounce++)
    {
        TraceRay(gScene.rtAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray.toRayDesc(), scatterRay);
        if (scatterRay.terminated)
            break;
        ray = Ray(scatterRay.origin, scatterRay.direction);
        scatterRay.pathLength++;
    }

    result[launchID] = float4(scatterRay.radiance, 1.0);
}

[shader("miss")]
void missMain(inout ScatterRayData scatterRay)
{
    scatterRay.radiance += scatterRay.thp * float3(gColor);
    scatterRay.terminated = true;
}

[shader("closesthit")]
void closestHitMain(inout ScatterRayData scatterRay, BuiltInTriangleIntersectionAttributes attribs)
{
    // Vertex interpolation
    VertexData vd = getVertexData(PrimitiveIndex(), attribs.barycentrics);

    // Shading data — geometry only (no normal map, no back-face flip)
    ShadingData hit = prepareShadingData(vd, WorldRayOrigin(), WorldRayDirection(), RayTCurrent());

    GLTFMaterial material = gScene.materials[hit.materialID];

#ifdef WEAK_WHITE_FURNACE
    // --- Furnace mode ---
    // Override material: metallic=1 + white baseColor gives F0=1 (F=1 everywhere).
    // Only roughness is preserved from the scene material.
    material.baseColor = float3(1, 1, 1);
    material.emissive = float3(0, 0, 0);
    material.metallic = 1.f;
    material.transmissionFactor = 0.f;
    material.baseColorTextureId = kInvalidTextureId;
    material.metallicTextureId = kInvalidTextureId;
    material.roughnessTextureId = kInvalidTextureId;
    material.emissiveTextureId = kInvalidTextureId;
    material.normalTextureId = kInvalidTextureId;
    material.transmissionTextureId = kInvalidTextureId;

    // Rebuild the shading frame from the flat face normal so the
    // shading hemisphere aligns with the geometric hemisphere used
    // by isValidScatter(). This isolates the BRDF energy loss from
    // smooth-normal / face-normal divergence.
    float3 N = hit.frontFacing ? hit.faceN : -hit.faceN;
    float3 t = hit.tangentW.xyz - dot(hit.tangentW.xyz, N) * N;
    hit.T = normalize(t);
    hit.B = hit.tangentW.w * cross(N, hit.T);
    hit.N = N;
#else
    // --- Production path ---

    // Emissive early-out
    float3 emissive = material.getEmissive(hit.uv);
    if (any(emissive > 0.f))
    {
        scatterRay.radiance += scatterRay.thp * emissive;
        scatterRay.terminated = true;
        return;
    }

    // Apply normal map and back-face flip
    material.prepareShadingFrame(hit);
#endif

    BSDFSample sample = material.scatter(hit, scatterRay.sg);

#ifdef WEAK_WHITE_FURNACE
    // Weak white furnace (Heitz 2014 Sec 5.2): single-bounce integral
    // against the constant environment. Accumulate weight directly and
    // terminate — no geometric validation or further path tracing.
    if (sample.pdf > 0.0f)
        scatterRay.radiance += scatterRay.thp * sample.weight * float3(gColor);
    scatterRay.terminated = true;
    return;
#else
    // Reject samples that cross the geometric surface (prevents light leaks)
    if (!isValidScatter(hit, sample))
    {
        scatterRay.terminated = true;
        return;
    }
#endif

    // Spawn next ray — reflection stays on the incident side, transmission offsets to the opposite geometric side.
    scatterRay.thp *= sample.weight;
    scatterRay.direction = sample.wo;
    float3 orientedFaceN = hit.getOrientedFaceNormal();
    scatterRay.origin = computeRayOrigin(hit.posW, sample.eventType == BSDFEventType.Reflection ? orientedFaceN : -orientedFaceN);
}
