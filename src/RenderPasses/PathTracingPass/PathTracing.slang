import Utils.Math.Ray;
import Utils.Sampling.SampleGenerator;
import Scene.Camera.Camera;
import Scene.Scene;

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    uint maxDepth;
    uint frameCount;
    float gColor;
};

ConstantBuffer<Camera> gCamera;
RWTexture2D<float4> result;

// Payload: defines what we pass between shaders
struct Payload
{
    float3 color;
    uint depth;
    TinyUniformSampleGenerator sampleGen;
};

[shader("raygeneration")] void rayGenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    if (launchID.x >= gWidth || launchID.y >= gHeight)
        return;

    float3 pixel = gCamera.data.pixel00 + launchID.x * gCamera.data.cameraU + launchID.y * gCamera.data.cameraV;
    Ray ray = Ray(gCamera.data.posW, normalize(pixel - gCamera.data.posW));

    Payload payload;
    payload.color = float3(0, 0, 0);
    payload.depth = 1;
    payload.sampleGen = TinyUniformSampleGenerator(launchID, frameCount);

    TraceRay(gScene.rtAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray.toRayDesc(), payload);

    result[launchID] = float4(payload.color, 1.0);
}

    [shader("miss")] void missMain(inout Payload payload)
{
    // Sky color as environment lighting
    payload.color = float3(gColor);
}

[shader("closesthit")] void closestHitMain(inout Payload payload, BuiltInTriangleIntersectionAttributes attribs)
{
    if (payload.depth >= maxDepth)
    {
        // If we've reached the maximum depth, return environment color
        payload.color = float3(gColor);
        return;
    }

    // Get barycentric coordinates
    float u = attribs.barycentrics.x;
    float v = attribs.barycentrics.y;
    float w = 1.0f - u - v;

    // Get primitive index (triangle index)
    uint primitiveIndex = PrimitiveIndex();

    // Get vertex indices for this triangle
    uint baseIndex = primitiveIndex * 3;
    uint i0 = gScene.indices[baseIndex + 0];
    uint i1 = gScene.indices[baseIndex + 1];
    uint i2 = gScene.indices[baseIndex + 2];

    // Get vertex normals
    float3 normal0 = gScene.vertices[i0].normal;
    float3 normal1 = gScene.vertices[i1].normal;
    float3 normal2 = gScene.vertices[i2].normal;
    float3 hitNormal = normalize(w * normal0 + u * normal1 + v * normal2);

    // Sample hemisphere for indirect lighting using the sample generator
    float2 randomSample = sampleNext2D(payload.sampleGen);
    float3 sampleDir = sampleHemisphere(hitNormal, randomSample);

    // Create new ray for indirect lighting
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    Ray bounceRay = Ray(hitPoint + hitNormal * 0.001f, sampleDir); // Small offset to avoid self-intersection

    // Create new sample generator for bounce ray to avoid correlation
    uint2 newPixel = uint2(primitiveIndex, payload.depth);

    // Trace bounce ray
    Payload bouncePayload;
    bouncePayload.color = float3(0, 0, 0);
    bouncePayload.depth = payload.depth + 1;
    bouncePayload.sampleGen = payload.sampleGen;

    TraceRay(gScene.rtAccel, RAY_FLAG_NONE, 0xFF, 0, 0, 0, bounceRay.toRayDesc(), bouncePayload);

    payload.color = 0.5 * bouncePayload.color;
}