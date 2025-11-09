#include "constants.hlsl"
#include "BRDF.hlsl"

TextureCube cubeMap : register(t0);
SamplerState samplerState : register(s0);
RWTexture2DArray<float4> prefilteredMap : register(u0);

cbuffer CB : register(b0)
{
	uint gCubeFaceSize;
	uint gMipLevel;
	uint2 _pad;
}

float3 FaceUVToDir(uint face, float2 uv)
{
	// Convert UV from [0,1] to [-1,1]
	float x = uv.x * 2.0f - 1.0f;
	float y = uv.y * 2.0f - 1.0f;

	// Cubemap face directions (standard D3D11 cubemap layout)
	switch (face)
	{
	case 0: return normalize(float3(1.0f, -y, -x));   // +X
	case 1: return normalize(float3(-1.0f, -y, x));   // -X
	case 2: return normalize(float3(x, 1.0f, y));     // +Y
	case 3: return normalize(float3(x, -1.0f, -y));   // -Y
	case 4: return normalize(float3(x, -y, 1.0f));    // +Z
	case 5: return normalize(float3(-x, -y, -1.0f));  // -Z
	default: return float3(0, 0, 1);
	}
}


[numthreads(16, 16, 1)]
void CS(uint3 tid : SV_DispatchThreadID)
{
	// Calculate current mip size
	uint currentMipSize = max(1, gCubeFaceSize >> gMipLevel);

	// Map pixel to UV coordinates 
	float2 uv = (float2(tid.xy) + 0.5f) / float(currentMipSize);

	// Calculate roughness based on mip level (0 = mirror, higher = rougher)
	float roughness = float(gMipLevel) / 4.0f; // Assuming 5 mip levels (0-4)
	roughness = clamp(roughness, 0.0f, 1.0f);

	// Process all 6 faces for this mip level
	for (uint face = 0; face < 6; ++face)
	{
		float3 N = FaceUVToDir(face, uv);
		float3 R = N;
		float3 V = R;

		const uint SAMPLE_COUNT = 1024;
		float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
		float TotalWeight = 0.0f;
		for (uint i = 0; i < SAMPLE_COUNT; i++)
		{
			// Generate sample using Hammersley sequence
			float2 Xi = Hammersley(i, SAMPLE_COUNT);
			float3 H = ImportanceSampleGGX(Xi, roughness, N);
			float3 L = 2.0f * dot(V, H) * H - V;

			float NoL = saturate(dot(N, L));

			if (NoL > 0.0f)
			{
				float3 sampleColor = cubeMap.SampleLevel(samplerState, L, 0).rgb * NoL;
				prefilteredColor += sampleColor;
				TotalWeight += NoL;
			}
		}
		prefilteredColor /= TotalWeight;
		prefilteredMap[uint3(tid.xy, face)] = float4(prefilteredColor, 1.0f);
	}
}