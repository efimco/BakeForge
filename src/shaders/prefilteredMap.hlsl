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
	float x = uv.x;
	float y = uv.y;

	// Each face assumes +Z forward in its local space; adjust per face:
	if (face == 0) return normalize(float3(1, -y, -x)); // +X
	if (face == 1) return normalize(float3(-1, -y, x)); // -X
	if (face == 2) return normalize(float3(x, 1, y)); // +Y
	if (face == 3) return normalize(float3(x, -1, -y)); // -Y
	if (face == 4) return normalize(float3(x, -y, 1)); // +Z
	/*face==5*/    return normalize(float3(-x, -y, -1)); // -Z
}

float2 Hammersley(uint i, uint N)
{
	float2 Xi;
	Xi.x = float(i) / float(N);
	uint bits = i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	Xi.y = float(bits) * 2.3283064365386963e-10; // / 0x100000000
	return Xi;
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	// Spherical to Cartesian
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// Tangent space to world space
	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangentX = normalize(cross(up, N));
	float3 tangentY = cross(N, tangentX);

	return normalize(tangentX * H.x + tangentY * H.y + N * H.z);
}

[numthreads(16, 16, 1)]
void CS(uint3 tid : SV_DispatchThreadID)
{
	// Calculate current mip size
	uint currentMipSize = max(1, gCubeFaceSize >> gMipLevel);
	
	if (tid.x >= currentMipSize || tid.y >= currentMipSize) return;
	
	// Map pixel to UV coordinates
	float2 uv = (float2(tid.xy) + 0.5f) / float(currentMipSize);
	uv = uv * 2.0f - 1.0f;
	
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
		float totalWeight = 0.0f;
		float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);

		for (uint sampleIndex = 0; sampleIndex < SAMPLE_COUNT; sampleIndex++)
		{
			// Generate sample using Hammersley sequence
			float2 Xi = Hammersley(sampleIndex, SAMPLE_COUNT);
			float3 H = ImportanceSampleGGX(Xi, N, roughness);
			float3 L = normalize(2.0f * dot(V, H) * H - V);

			float NdotL = max(dot(N, L), 0.0f);
			if (NdotL > 0.0f)
			{
				// Sample environment map with proper mip level for filtering
				float D = distributionGGX(N, H, roughness);
				float NdotH = max(dot(N, H), 0.0f);
				float HdotV = max(dot(H, V), 0.0f);
				float pdf = D * NdotH / (4.0f * HdotV) + 0.0001f;
				
				// Calculate proper LOD for sampling
				float resolution = 512.0f; // Base cubemap resolution
				float saTexel = 4.0f * PI / (6.0f * resolution * resolution);
				float saSample = 1.0f / (float(SAMPLE_COUNT) * pdf + 0.0001f);
				float mipLevel = roughness == 0.0f ? 0.0f : 0.5f * log2(saSample / saTexel);
				
				prefilteredColor += cubeMap.SampleLevel(samplerState, L, mipLevel).rgb * NdotL;
				totalWeight += NdotL;
			}
		}

		if (totalWeight > 0.0f)
		{
			prefilteredColor /= totalWeight;
		}

		// Write to the correct face and mip level
		// Note: The UAV should be created for the specific mip level
		prefilteredMap[uint3(tid.xy, face)] = float4(prefilteredColor, 1.0f);
	}
}