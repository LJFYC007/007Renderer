import Scene.Camera.CameraData;
import Utils.Math.Ray;

struct Camera
{
    CameraData data;

    Ray computeRayPinhole(uint2 pixel, bool applyJitter = true)
    {
        float3 pixelPos = data.pixel00 + pixel.x * data.cameraU + pixel.y * data.cameraV;

        if (applyJitter)
            pixelPos += data.jitter.x * data.cameraU + data.jitter.y * data.cameraV;

        Ray ray = Ray(data.posW, normalize(pixelPos - data.posW));
        return ray;
    }
};