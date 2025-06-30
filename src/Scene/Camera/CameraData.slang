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
    float _padding0; // Padding to ensure proper alignment
    float3 up;
    float _padding1; // Padding to ensure proper alignment
};
