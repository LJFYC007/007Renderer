import "Utils/Math/Ray.slang";
import "Utils/Math/Sampling.slang";
import "Scene/Camera/Camera.slang";

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
RaytracingAccelerationStructure gScene;

// Vertex structure that matches C++ side
struct Vertex
{
    float3 position;
    float2 texCoord;
    float3 normal;
};

// Vertex and index buffers
StructuredBuffer<Vertex> gVertices;
StructuredBuffer<uint> gIndices;

// Payload: defines what we pass between shaders
struct Payload
{
    float3 color;
    uint depth;
    uint seed;
};

[shader("raygeneration")]
void rayGenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    if (launchID.x >= gWidth || launchID.y >= gHeight)
        return;    

    // Better seed initialization with multiple hash steps for improved distribution
    uint2 pixelCoord = launchID.xy;
    uint pixelIndex = pixelCoord.y * gWidth + pixelCoord.x;
    
    // Use multiple hash operations to break correlation patterns
    uint baseSeed = hash(pixelIndex);
    baseSeed = hash(baseSeed + frameCount * 0x9e3779b9u);
    baseSeed = wangHash(baseSeed);
    
    float3 pixel = gCamera.data.pixel00 + launchID.x * gCamera.data.cameraU + launchID.y * gCamera.data.cameraV;
    Ray ray = Ray(gCamera.data.posW, normalize(pixel - gCamera.data.posW));

    Payload payload;
    payload.color = float3(0, 0, 0);
    payload.depth = 1;
    payload.seed = baseSeed;
    
    TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray.toRayDesc(), payload);

    result[launchID] = float4(payload.color, 1.0);
}

[shader("miss")]
void missMain(inout Payload payload)
{
    // Sky color as environment lighting
    payload.color = float3(gColor);
}

[shader("closesthit")]
void closestHitMain(inout Payload payload, BuiltInTriangleIntersectionAttributes attribs)
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
    uint i0 = gIndices[baseIndex + 0];
    uint i1 = gIndices[baseIndex + 1];
    uint i2 = gIndices[baseIndex + 2];
    
    // Get vertex normals
    float3 normal0 = gVertices[i0].normal;
    float3 normal1 = gVertices[i1].normal;
    float3 normal2 = gVertices[i2].normal;
    float3 hitNormal = normalize(w * normal0 + u * normal1 + v * normal2);    // Sample hemisphere for indirect lighting with decorrelated random numbers
    uint sampleSeed1 = nextRandom(payload.seed);
    uint sampleSeed2 = nextRandom(payload.seed);
    float2 randomSample = float2(
        float(sampleSeed1) / float(0xffffffffu),
        float(sampleSeed2) / float(0xffffffffu)
    );
    float3 sampleDir = sampleHemisphere(hitNormal, randomSample);
    
    // Create new ray for indirect lighting
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    Ray bounceRay = Ray(hitPoint + hitNormal * 0.001f, sampleDir);  // Small offset to avoid self-intersection

    // Create new seed for bounce ray to avoid correlation
    // Hash with depth and primitive index for better distribution
    uint bounceSeed = hash(payload.seed + primitiveIndex * 1023u + payload.depth * 7919u);
    bounceSeed = wangHash(bounceSeed);
    
    // Trace bounce ray
    Payload bouncePayload;
    bouncePayload.color = float3(0, 0, 0);
    bouncePayload.depth = payload.depth + 1;
    bouncePayload.seed = bounceSeed;
    
    TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, bounceRay.toRayDesc(), bouncePayload);
    
    payload.color = 0.5 * bouncePayload.color;
    payload.seed = bouncePayload.seed;
}