#ifndef LUT4BRDF_HLSL
#define LUT4BRDF_HLSL
#include "constants.hlsl"
#include "BRDF.hlsl"

RWTexture2D<float2> brdfLUT : register(u0);


float2 IntegrateBRDF(float NdotV, float roughness)
{
	NdotV = saturate(NdotV);
	roughness = saturate(roughness);

	if (roughness < 0.0001)
		return float2(0.0, 0.0);

	float3 V;
	V.x = sqrt(1.0 - (NdotV * NdotV)); // sin
	V.y = 0.0;
	V.z = NdotV; // cos

	float A = 0.0;
	float B = 0.0;
	float3 N = float3(0.0, 0.0, 1.0);

	const uint NumSamples = 1024u;
	for (uint i = 0u; i < NumSamples; ++i)
	{
		// Generate Hammersley sample
		float2 Xi = Hammersley(i, NumSamples);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2.0 * dot(V, H) * H - V;

		float NoL = saturate(L.z);
		float NoH = saturate(H.z);
		float VoH = saturate(dot(V, H));

		if (NoL > 0.0)
		{
			float G = geometrySchlickGGX_IBL(NoL, NdotV, roughness);

			float Gvis = (G * VoH) / (NoH * NdotV);
			float Fc = pow(1.0 - VoH, 5.0);

			A += (1.0 - Fc) * Gvis;
			B += Fc * Gvis;
		}
	}

	return float2(A, B) / float(NumSamples);
}

[numthreads(8, 8, 1)]
void CS(uint2 tid : SV_DispatchThreadID)
{

	// Convert pixel coordinates to normalized [0,1] range
	uint w, h;
	brdfLUT.GetDimensions(w, h);

	if (tid.x >= w || tid.y >= h)
		return;

	float2 uv = (float2(tid.xy) + 0.5) / float2(w, h);
	uv.x += 0.02;
	float NdotV = uv.x;
	float roughness = uv.y;

	brdfLUT[tid] = IntegrateBRDF(NdotV, roughness);
}

#endif // LUT4BRDF_HLSL


