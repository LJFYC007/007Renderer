import Scene.Scene;
import Scene.VertexData;
import Scene.Material.GLTFMaterial;
import Utils.Sampling.SampleGeneratorInterface;

struct LightSample
{
    float3 position; // Sampled point on the emissive triangle
    float3 normal;   // Geometric face normal at the sampled point
    float3 emissive; // Emissive radiance Le at the sampled point
    float pdf;       // PDF in area measure: 1 / totalEmissiveArea
    bool valid;
};

// Sample a random point on a random emissive triangle using area-weighted CDF.
// pdf is in area measure (constant for area-weighted sampling): 1 / totalEmissiveArea.
LightSample sampleLight<S : ISampleGenerator>(uint emissiveTriangleCount, float totalEmissiveArea, inout S sg)
{
    LightSample ls;
    ls.valid = false;
    ls.pdf = 0.f;
    ls.position = float3(0);
    ls.normal = float3(0);
    ls.emissive = float3(0);

    if (emissiveTriangleCount == 0 || totalEmissiveArea <= 0.f)
        return ls;

    // Binary search the area-weighted CDF to select a triangle
    float u = sampleNext1D(sg);
    uint lo = 0;
    uint hi = emissiveTriangleCount - 1;
    while (lo < hi)
    {
        uint mid = (lo + hi) / 2;
        if (gScene.emissiveTriangles[mid].cdfUpper <= u)
            lo = mid + 1;
        else
            hi = mid;
    }
    uint triIdx = gScene.emissiveTriangles[lo].triangleIndex;

    // Sample a uniform random point on the triangle (Turk 1990)
    float2 u2 = sampleNext2D(sg);
    float su = sqrt(u2.x);
    float baryU = 1.f - su;
    float baryV = u2.y * su;

    VertexData vd = getVertexData(triIdx, float2(baryU, baryV));
    ls.position = vd.posW;
    ls.normal = vd.faceNormalW;
    ls.emissive = gScene.materials[vd.materialID].getEmissive(vd.uv);

    // Area-weighted sampling: pdf_area = (area_i / totalArea) * (1 / area_i) = 1 / totalArea
    ls.pdf = 1.f / totalEmissiveArea;
    ls.valid = true;

    return ls;
}

// Evaluate the light sampler's solid-angle PDF for a BSDF-scattered ray that hit an emissive surface.
//
// With area-weighted sampling the area-measure PDF is constant: 1 / totalEmissiveArea.
// Convert area measure to solid angle:
//   pdf_w = pdf_a * dist^2 / |cos(theta_light)|
// where theta_light is the angle between the light normal and the direction from light to shading point.
float evalLightPdf(uint emissiveTriangleCount, float totalEmissiveArea, float3 shadingPos, float3 lightPos, float3 lightNormal)
{
    if (emissiveTriangleCount == 0 || totalEmissiveArea <= 0.f)
        return 0.f;

    float3 toLight = lightPos - shadingPos;
    float dist2 = dot(toLight, toLight);
    // Double-sided emission: use abs() so both faces contribute a valid PDF.
    float cosLight = abs(dot(lightNormal, normalize(-toLight)));

    if (cosLight < 1e-8f)
        return 0.f;

    return dist2 / (totalEmissiveArea * cosLight);
}
