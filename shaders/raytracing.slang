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
RaytracingAccelerationStructure gScene;

// Payload: defines what we pass between shaders
struct Payload
{
    float3 color;
    float2 pixel;
};

[shader("raygeneration")]
void rayGenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    if (launchID.x >= gWidth || launchID.y >= gHeight)
        return;    float3 pixel = gCamera.data.pixel00 + launchID.x * gCamera.data.cameraU + launchID.y * gCamera.data.cameraV;
    Ray ray = Ray(gCamera.data.posW, normalize(pixel - gCamera.data.posW));

    Payload payload;
    payload.color = float3(0, 0, 0); // Initialize to black
    payload.pixel = float2(float(launchID.x) / float(gWidth), float(launchID.y) / float(gHeight)); // Store normalized pixel coordinates

    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.dir;
    rayDesc.TMin = 0.001;
    rayDesc.TMax = 10000.0;

    TraceRay(gScene,
        RAY_FLAG_NONE,
        0xFF,
        0,
        0,
        0,
        rayDesc,
        payload);

    result[launchID] = float4(payload.color, 1.0);
}

[shader("miss")]
void missMain(inout Payload payload)
{
    // Create a gradient based on ray direction for visual debugging
    payload.color = float3(payload.pixel.x, payload.pixel.y, gColor); 
}

[shader("closesthit")]
void closestHitMain(inout Payload payload, BuiltInTriangleIntersectionAttributes attribs)
{
    // Use barycentric coordinates to create a color pattern
    float u = attribs.barycentrics.x;
    float v = attribs.barycentrics.y;
    float w = 1.0f - u - v;
    payload.color = float3(u, v, w); // RGB based on barycentric coordinates
}