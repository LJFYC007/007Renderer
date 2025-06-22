struct Ray
{
    float3 origin;
    float3 dir;

    __init(float3 origin, float3 dir)
    {
        this.origin = origin;
        this.dir = dir;
    }

    float3 eval(float t) { return origin + t * dir; }
}