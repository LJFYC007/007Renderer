#include "Utils/Math/MathConstants.slangh"

/*
    \param[in] cosTheta Dot product between shading normal and half-vector
    \param[in] alpha2 Squared GGX parameter (squared alpha)
*/
float evalNdfGGX(float cosTheta, float alpha2)
{
    float denom = (cosTheta * cosTheta) * (alpha2 - 1.f) + 1.f;
    return alpha2 / (PI * denom * denom);
}

/*
    \param[in] cosTheta Dot product between shading normal and evaluated direction
    \param[in] alpha2 Squared GGX parameter (squared alpha)
*/
float evalG1GGX(float cosTheta, float alpha2)
{
    float cosTheta2 = cosTheta * cosTheta;
    float tanThetaSqr = max(1 - cosTheta2, 0) / cosTheta2;
    return 2 / (1 + sqrt(1 + alpha2 * tanThetaSqr));
}

float schlickPow5(float x)
{
    float x2 = x * x;
    return x2 * x2 * x;
}

float3 evalFresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.f - F0) * schlickPow5(clamp(1.f - abs(cosTheta), 0.f, 1.f));
}

/*
    Exact unpolarized dielectric Fresnel reflectance.
    \param[in] cosThetaI Cosine of the incident angle, measured in the incident
 * medium
    \param[in] eta Ratio of indices of refraction eta_i / eta_t
*/
float evalFresnelDielectric(float cosThetaI, float eta)
{
    float cosI = clamp(abs(cosThetaI), 0.0f, 1.0f);
    float sin2T = eta * eta * max(0.0f, 1.0f - cosI * cosI);
    if (sin2T >= 1.0f)
        return 1.0f;

    float cosT = sqrt(max(0.0f, 1.0f - sin2T));
    float rs = (eta * cosI - cosT) / (eta * cosI + cosT);
    float rp = (cosI - eta * cosT) / (cosI + eta * cosT);
    return 0.5f * (rs * rs + rp * rp);
}

float3 sampleGGXHalfVector(float2 u, float alpha2)
{
    float cosTheta = sqrt((1.0f - u.x) / (1.0f + (alpha2 - 1.0f) * u.x));
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = TWO_PI * u.y;
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}
