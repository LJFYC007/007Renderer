import "Utils/Math/Ray.slang";
import "Scene/Camera/Camera.slang";

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    float gColor;
};

ConstantBuffer<Camera> gCamera;
RWTexture2D<float4> result;

bool hitSphere(Ray ray)
{
    float3 center = float3(0.0, 0.0, -1.0);
    float radius = 0.5f;

    float3 oc = ray.origin - center;
    float a = dot(ray.dir, ray.dir);
    float b = 2.0 * dot(oc, ray.dir);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    return (discriminant >= 0);
}

float3 rayColor(Ray ray)
{
    if (hitSphere(ray))
        return float3(0.f, 1.f, 0.f);
    float a = 0.5 * (normalize(ray.dir).y + 1.0);
    return (1.0 - a) * gColor;
}

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    if (pixelCoord.x >= gWidth || pixelCoord.y >= gHeight)
        return;
    uint id = pixelCoord.y * gWidth + pixelCoord.x;

    float3 pixel = gCamera.data.pixelCenter + pixelCoord.x * gCamera.data.cameraU + pixelCoord.y * gCamera.data.cameraV;

    Ray ray = Ray(gCamera.data.posW, pixel - gCamera.data.posW);
    result[pixelCoord] = float4(rayColor(ray), 1.f);
}
