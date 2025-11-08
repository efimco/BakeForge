#include "constants.hlsl"
#include "BRDF.hlsl"

RWTexture2D<float4> brdfLUT : register(u0);

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
	if (DTid.x >= gCubeFaceSize || DTid.y >= gCubeFaceSize) return;
	float2 uv = (float2(DTid.xy) + 0.5f) / float(gCubeFaceSize);
	uv = uv * 2.0f - 1.0f;

	for (int i = 0; i < 6; i++)
	{
		float3 N = FaceUVToDir(i, uv);

		float3 R = N;
		float3 V = R;
		// Sample the environment map
		const uint SAMPLE_COUNT = 1024u;
		float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
		float totalWeight = 0.0f;

		for (uint j = 0u; j < SAMPLE_COUNT; ++j)
		{
			float2 Xi = Hammersley(j, SAMPLE_COUNT);
			float3 H = ImportanceSampleGGX(Xi, N, float(gMipLevel) / float(MAX_MIP_LEVELS - 1));
			float3 L = normalize(2.0f * dot(V, H) * H - V);

			float NdotL = max(dot(N, L), 0.0f);
			if (NdotL > 0.0f)
			{
				prefilteredColor += cubeMap.SampleLevel(samplerState, L, 0).rgb * NdotL;
				totalWeight += NdotL;
			}
		}
		prefilteredColor /= totalWeight;

		prefilteredMap[uint3(DTid.xy, i)] = float4(prefilteredColor, 1.0f);
	}
}