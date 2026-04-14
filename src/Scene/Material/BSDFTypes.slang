// Lives here (not in ShadingData.slang) so ShadingData stays geometry-only.

import Scene.ShadingData;

enum class BSDFEventType
{
    Reflection = 0,
    Transmission = 1,
};

struct BSDFSample
{
    float3 wo;     // Outgoing direction in world space
    float pdf;     // Full-mixture solid-angle pdf for wo, consistent with evalPdf()
    float3 weight; // eval(wi, wo) / evalPdf(wi, wo), i.e. f * |cosTheta_o| / pdf
    BSDFEventType eventType;
};

// Validates wo against the geometric hemisphere (not the shading hemisphere).
// Reflection: wo on the same side as the incoming ray. Transmission: opposite side.

bool isValidScatter(const ShadingData sd, const BSDFSample sample)
{
    if (sample.pdf <= 0.0f)
        return false;
    float3 orientedFaceN = sd.getOrientedFaceNormal();
    if (sample.eventType == BSDFEventType.Reflection)
        return dot(sample.wo, orientedFaceN) > 0.0f;
    else
        return dot(sample.wo, orientedFaceN) < 0.0f;
}
