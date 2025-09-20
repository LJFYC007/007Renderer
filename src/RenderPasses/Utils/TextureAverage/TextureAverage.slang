cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
};

Texture2D<float4> inputTexture;
RWStructuredBuffer<float4> resultBuffer;

static const uint kTileWidth = 16;
static const uint kTileHeight = 16;

// Each thread integrates one tile of the texture and writes its partial sum.
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    const uint tilesX = (gWidth + kTileWidth - 1) / kTileWidth;
    const uint tilesY = (gHeight + kTileHeight - 1) / kTileHeight;

    if (dispatchThreadId.x >= tilesX || dispatchThreadId.y >= tilesY)
        return;

    const uint tileX = dispatchThreadId.x;
    const uint tileY = dispatchThreadId.y;

    const uint startX = tileX * kTileWidth;
    const uint startY = tileY * kTileHeight;
    const uint endX = min(startX + kTileWidth, gWidth);
    const uint endY = min(startY + kTileHeight, gHeight);

    float4 tileSum = float4(0, 0, 0, 0);
    for (uint y = startY; y < endY; ++y)
        for (uint x = startX; x < endX; ++x)
            tileSum += inputTexture[uint2(x, y)];

    const uint linearIndex = tileY * tilesX + tileX;
    resultBuffer[linearIndex] = tileSum;
}
