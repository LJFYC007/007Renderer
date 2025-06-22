import "Utils/Math/Ray.slang";
import "Scene/Camera/Camera.slang";

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    float gColor;
};

ConstantBuffer<Camera> gCamera;
RWTexture2D<float3> result;

float3 rayColor(Ray ray)
{
    float a = 0.5 * (ray.dir.y + 1.0);
    return (1.0 - a) * float3(1.0, 1.0, 1.0) + a * float3(0.0, 0.0, 0.0);
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

    Ray ray = Ray(gCamera.data.posW, normalize(pixel - gCamera.data.posW));
    result[pixelCoord] = rayColor(ray);
}
