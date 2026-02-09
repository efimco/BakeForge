
cbuffer CB : register(b0)
{
	uint Height;
	uint Width;
	uint TileSize;
	uint TileNumX;
	uint TileNumY;
	uint NumTiles;
}

// For now we only read from rayDirectionBlend texture - those are F32 masks.
// If you in the future would ever need to diff textures with multiple components -
// convert this to a normal Texture2D
Texture2D<float> TextureA : register(t0);
Texture2D<float> TextureB : register(t1);

// An output buffer that receives tile indices - its preallocated to be NumTiles
RWStructuredBuffer<uint> TileIndices : register(u0);

// Shared variable to hold per-tile diff flag
groupshared uint tileDiff;

[numthreads(8, 8, 1)]
void CS(uint3 GTid : SV_GroupThreadID,
		uint3 Gid  : SV_GroupID)
{
	// Pixels per thread
	uint px = TileSize / 8;

	// Tile origin in pixel coordinates
	uint2 tileOrigin = Gid.xy * TileSize;

	// Each thread handles an px x px tile
	uint2 threadOrigin = tileOrigin + GTid.xy * px;

	// Initialize shared tileDiff once per group
	if (all(GTid.xy == 0))
		tileDiff = 0;

	GroupMemoryBarrierWithGroupSync();

	for (uint y = 0; y < px; ++y)
	{
		for (uint x = 0; x < px; ++x)
		{
			uint2 pixel = threadOrigin + uint2(x, y);

			// Skip pixels outside the texture bounds
			if (pixel.x >= Width || pixel.y >= Height)
				continue;

			float a = TextureA.Load(int3(pixel, 0));
			float b = TextureB.Load(int3(pixel, 0));

			// If any channel differs, mark tile as changed
			if (any(a != b))
			{
			    InterlockedOr(tileDiff, 1);
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	// Only one thread writes the tileDiff flag per tile
	if (all(GTid.xy == 0))
	{
		uint tileIndex = Gid.y * TileNumX + Gid.x;
		TileIndices[tileIndex] = tileDiff > 0 ? tileIndex : 0;
	}
}