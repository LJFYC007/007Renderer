import Utils.Math.Ray;
import Utils.Sampling.SampleGeneratorInterface;
import Scene.ShadingData;

struct BSDFSample
{
    float3 wo;     // Sampled direction in world space (normalized).
    float pdf;     // pdf with respect to solid angle for the sampled direction (wo).
    float3 weight; // Sample weight f(wi, wo) * dot(wo, n) / pdf(wo).
};

struct BSDFProperties
{
    float3 emission;
};

interface Material
{
    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg);
}