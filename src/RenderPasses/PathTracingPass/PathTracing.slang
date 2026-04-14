import Utils.Math.Ray;
import Utils.Sampling.SampleGenerator;
import Scene.Camera.Camera;
import Scene.Scene;
import Scene.VertexData;
import Scene.ShadingData;
import Scene.ShadingPrep;
import Scene.Material.BSDFTypes;
import Scene.Material.GLTFMaterial;
import RenderPasses.PathTracingPass.LightSampler;

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    uint maxDepth;
    uint frameCount;
    float gColor;
    uint emissiveTriangleCount;
    float totalEmissiveArea;
    uint _cbPadding;
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
    float prevBsdfPdf; // PDF of the BSDF sample that generated this ray (for MIS at next emissive hit)
    float3 prevPos;    // Position of the previous shading point (for evalLightPdf at next emissive hit)

    TinyUniformSampleGenerator sg; // Per-ray state for the sample generator (up to 16B)

    __init(TinyUniformSampleGenerator sg)
    {
        this.terminated = false;
        this.pathLength = 0;
        this.radiance = float3(0, 0, 0);
        this.thp = float3(1, 1, 1);
        this.origin = float3(0, 0, 0);
        this.direction = float3(0, 0, 0);
        this.prevBsdfPdf = 0.0f;
        this.prevPos = float3(0, 0, 0);
        this.sg = sg;
    }
};

struct ShadowRayData
{
    bool visible;
};

// Trace a shadow ray for visibility between origin and target.
// Both endpoints are offset along their respective normals to avoid
// self-intersection (see PBRT4 SpawnRayTo with two interactions).
bool traceVisibilityRay(float3 origin, float3 target, float3 originNormal, float3 targetNormal)
{
    float3 offsetOrigin = computeRayOrigin(origin, originNormal);
    float3 offsetTarget = computeRayOrigin(target, targetNormal);

    float3 toTarget = offsetTarget - offsetOrigin;
    float dist = length(toTarget);
    float3 dir = toTarget / dist;

    ShadowRayData shadowRay;
    shadowRay.visible = false;

    TraceRay(
        gScene.rtAccel,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xFF,
        0, // RayContributionToHitGroupIndex = 0 (reuse scatter hit group, closest hit is skipped)
        0, // MultiplierForGeometryContributionToHitGroupIndex
        1, // MissShaderIndex = 1 (shadowMissMain)
        Ray(offsetOrigin, dir, 0.0f, dist).toRayDesc(),
        shadowRay
    );

    return shadowRay.visible;
}

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
    // Environment is not in the light sampler, so MIS weight for BSDF = 1.0 implicitly.
    scatterRay.radiance += scatterRay.thp * float3(gColor);
    scatterRay.terminated = true;
}

[shader("miss")]
void shadowMissMain(inout ShadowRayData shadowRay)
{
    shadowRay.visible = true;
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

    BSDFSample sample = material.scatter(hit, scatterRay.sg);

    // Weak white furnace (Heitz 2014 Sec 5.2): single-bounce integral
    // against the constant environment. Accumulate weight directly and
    // terminate — no geometric validation or further path tracing.
    if (sample.pdf > 0.0f)
        scatterRay.radiance += scatterRay.thp * sample.weight * float3(gColor);
    scatterRay.terminated = true;
    return;
#else
    float3 emissive = material.getEmissive(hit.uv);

    if (any(emissive > 0.f))
    {
        if (scatterRay.pathLength == 0)
        {
            // Bounce 0: camera directly sees the light. Accumulate without MIS.
            scatterRay.radiance += scatterRay.thp * emissive;
        }
        else
        {
            // NEE samples lights from both hemispheres, so lightPdf is always evaluated
            // regardless of the previous scatter event type.
            float lightPdf = evalLightPdf(emissiveTriangleCount, totalEmissiveArea, scatterRay.prevPos, hit.posW, vd.faceNormalW);
            float bsdfPdf = scatterRay.prevBsdfPdf;
            float misWeight = (bsdfPdf + lightPdf > 0.f) ? bsdfPdf / (bsdfPdf + lightPdf) : 0.f;
            scatterRay.radiance += scatterRay.thp * emissive * misWeight;
        }
        scatterRay.terminated = true;
        return;
    }

    material.prepareShadingFrame(hit);

    float3 orientedFaceN = hit.getOrientedFaceNormal();

    if (scatterRay.pathLength >= maxDepth)
    {
        scatterRay.terminated = true;
        return;
    }

    // Sample all material textures once; reuse for NEE eval/evalPdf and for the scatter sample.
    GLTFBSDF bsdf = material.prepareBSDF(hit);

    if (emissiveTriangleCount > 0)
    {
        LightSample ls = sampleLight(emissiveTriangleCount, totalEmissiveArea, scatterRay.sg);
        if (ls.valid && ls.pdf > 0.f)
        {
            float3 toLight = ls.position - hit.posW;
            float dist2 = dot(toLight, toLight);
            float dist = sqrt(dist2);
            float3 wi = toLight / dist;

            // Use the geometric normal (oriented face normal) for the sidedness test,
            // not the shading normal. On normal-mapped surfaces the shading normal
            // can flip into the opposite geometric hemisphere, which would offset the
            // shadow-ray origin to the wrong side of the triangle.
            float cosGeom = dot(wi, orientedFaceN);
            // Double-sided emission: accept light from either face.
            float cosRaw = dot(ls.normal, -wi);
            float cosAtLight = abs(cosRaw);

            if (abs(cosGeom) > 1e-8f && cosAtLight > 1e-8f)
            {
                // Orient the light normal toward the shading point, and the shadow-ray
                // origin toward the light side of the surface. Offsetting only the
                // origin causes false occlusion for nearly-coplanar light/shading points.
                float3 lightOffsetN = cosRaw > 0.f ? ls.normal : -ls.normal;
                float3 shadowOriginN = cosGeom > 0.f ? orientedFaceN : -orientedFaceN;
                bool visible = traceVisibilityRay(hit.posW, ls.position, shadowOriginN, lightOffsetN);
                if (visible)
                {
                    float3 wiLocal = hit.toLocal(wi);
                    float3 bsdfVal = bsdf.eval(wiLocal, scatterRay.sg);
                    float lightPdfW = ls.pdf * dist2 / cosAtLight;
                    float bsdfPdf = bsdf.evalPdf(wiLocal);
                    float misWeight = lightPdfW / (lightPdfW + bsdfPdf);
                    scatterRay.radiance += scatterRay.thp * ls.emissive * bsdfVal / lightPdfW * misWeight;
                }
            }
        }
    }

    BSDFSample sample = bsdf.sample(scatterRay.sg);
    // Rewrite per-lobe pdf/weight to full-mixture values so next-bounce MIS
    // uses a pdf consistent with evalLightPdf's solid-angle convention.
    if (sample.pdf > 0.f)
    {
        float mixPdf = bsdf.evalPdf(sample.wo);
        if (mixPdf > 0.f)
        {
            sample.weight = bsdf.eval(sample.wo, scatterRay.sg) / mixPdf;
            sample.pdf = mixPdf;
        }
        else
        {
            sample.pdf = 0.f;
            sample.weight = float3(0.f);
        }
    }
    sample.wo = hit.toWorld(sample.wo);

    if (!isValidScatter(hit, sample))
    {
        scatterRay.terminated = true;
        return;
    }

    scatterRay.prevBsdfPdf = sample.pdf;
    scatterRay.prevPos = hit.posW;
    scatterRay.thp *= sample.weight;
    scatterRay.direction = sample.wo;
    scatterRay.origin = computeRayOrigin(hit.posW, sample.eventType == BSDFEventType.Reflection ? orientedFaceN : -orientedFaceN);
#endif
}
