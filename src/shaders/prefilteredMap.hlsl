#include "constants.hlsl"
#include "BRDF.hlsl"

TextureCube cubeMap : register(t0);
SamplerState samplerState : register(s0);
RWTexture2DArray<float4> prefilteredMap : register(u0);

cbuffer CB : register(b0)
{
	uint gMipLevelSize;
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

static const uint SAMPLE_COUNT = 1024u * 4u;

float3 PrefilterEnvMap(float roughness, float3 R)
{
	float3 N = R;
	float3 V = R;
	if (roughness <= 0.0001f)
		return cubeMap.SampleLevel(samplerState, R, 0).rgb;

	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
	float TotalWeight = 0.0001f;

	for (uint i = 0; i < SAMPLE_COUNT; i++)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, roughness, R);
		float3 L = 2.0f * dot(V, H) * H - V;
		float NoL = saturate(dot(N, L));

		if (NoL > 0.0)
		{
			prefilteredColor += cubeMap.SampleLevel(samplerState, L, 0).rgb * NoL;
			TotalWeight += NoL;
		}
	}
	return prefilteredColor / TotalWeight;
}


[numthreads(32, 32, 1)]
void CS(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= gMipLevelSize || tid.y >= gMipLevelSize)
		return;

	// Map pixel to UV coordinates 
	float2 uv = (float2(tid.xy) + 0.5f) / float2(gMipLevelSize, gMipLevelSize);

	// Calculate roughness based on mip level (0 = mirror, higher = rougher)
	float roughness = float(gMipLevel) / 7.0f; // Assuming 8 mip levels (0-7)
	roughness = roughness * roughness;

	// Process all 6 faces for this mip level
	for (uint face = 0; face < 6; ++face)
	{
		float3 R = FaceUVToDir(face, uv);
		float3 prefilteredColor = PrefilterEnvMap(roughness, R);
		prefilteredMap[uint3(tid.xy, face)] = float4(prefilteredColor, 1.0f);
	}
}