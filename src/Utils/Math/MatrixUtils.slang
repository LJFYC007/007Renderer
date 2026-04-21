// Helpers for 3x4 affine localToWorld matrices.

float3 transformPoint(float3x4 M, float3 p)
{
    return mul(M, float4(p, 1.0f));
}

// Transform a unit normal by (M^{-1})^T using cofactor rows of M's linear part.
// The raw cofactor product includes a factor of det(M); we absorb |det| via the
// final normalize and sign(det) via `mirrorSign`, so the returned direction is
// the formal inverse-transpose answer even under mirrored transforms.
// `mirrorSign` is exposed so callers can compose TBN handedness without redoing
// the cofactor work.
float3 transformNormal(float3x4 M, float3 nLocal, out float mirrorSign)
{
    float3 r0 = float3(M[0][0], M[0][1], M[0][2]);
    float3 r1 = float3(M[1][0], M[1][1], M[1][2]);
    float3 r2 = float3(M[2][0], M[2][1], M[2][2]);
    float3 cof0 = cross(r1, r2);
    float3 cof1 = cross(r2, r0);
    float3 cof2 = cross(r0, r1);
    mirrorSign = dot(r0, cof0) < 0.0f ? -1.0f : 1.0f;
    return normalize(mirrorSign * float3(dot(cof0, nLocal), dot(cof1, nLocal), dot(cof2, nLocal)));
}
