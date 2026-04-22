import Scene.VertexData;
import Scene.ShadingData;

// Geometry-only. Does NOT apply normal maps or flip for back-face.
// Back-face handling is a material decision — belongs in prepareShadingFrame.
ShadingData prepareShadingData(VertexData vd, float3 rayOrigin, float3 rayDir, float rayT)
{
    ShadingData sd;
    sd.posW = rayOrigin + rayDir * rayT;
    sd.V = normalize(rayOrigin - sd.posW);
    sd.uv = vd.uv;

    sd.faceN = vd.faceNormalW;
    sd.tangentW = vd.tangentW;
    sd.materialID = vd.materialID;

    sd.frontFacing = dot(vd.faceNormalW, rayDir) <= 0.0f;

    sd.buildTBN(vd.normalW);

    return sd;
}
