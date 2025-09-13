#include "Utils/Math/MathConstants.slangh"

/*
    \param[in] cosTheta Dot product between shading normal and half-vector
    \param[in] alpha2 Squared GGX parameter (squared alpha)
*/
float evalNdfGGX(float cosTheta, float alpha2)
{
    if (cosTheta <= 0.f)
        return 0.f;
    float denom = (cosTheta * cosTheta) * (alpha2 - 1.f) + 1.f;
    return alpha2 / (PI * denom * denom);
}

/*
    \param[in] cosTheta Dot product between shading normal and evaluated direction
    \param[in] alpha2 Squared GGX parameter (squared alpha)
*/
float evalG1GGX(float cosTheta, float alpha2)
{
    if (cosTheta <= 0.f)
        return 0.f;
    float cosTheta2 = cosTheta * cosTheta;
    float tanThetaSqr = max(1 - cosTheta2, 0) / cosTheta2;
    return 2 / (1 + sqrt(1 + alpha2 * tanThetaSqr));
}

/*
    \param[in] cosTheta Dot product between view direction and half-vector
    \param[in] F0 Specular reflectance at normal incidence
*/
float3 evalFresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.f - F0) * pow(clamp(1.f - abs(cosTheta), 0.f, 1.f), 5.f);
}