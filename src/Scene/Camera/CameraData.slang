struct CameraData
{
    float3 posW;
    float focalLength;
    float3 pixel00;
    float aspectRatio;
    float3 cameraU;
    uint frameWidth;
    float3 cameraV;
    uint frameHeight;
    float3 target;
    float fovY;
    float3 forward;
    float moveSpeed;
    float3 right;
    float _padding1;
    float3 up;
    bool enableJitter;
    float2 jitter;
    float2 _padding2;
};
