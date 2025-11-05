#include "constants.hlsl"
TextureCube cubeMap : register(t0);
SamplerState samplerState : register(s0);
RWTexture2DArray<float4> irradianceMap : register(u0);

cbuffer CB : register(b0)
{
	uint gCubeFaceSize;
	uint3 _pad;
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


[numthreads(8, 8, 1)]
void CS(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= gCubeFaceSize || tid.y >= gCubeFaceSize) return;
	float2 uv = (float2(tid.xy) + 0.5f) / float(gCubeFaceSize);
	uv = uv * 2.0f - 1.0f;

	for (int i = 0; i < 6; i++)
	{
		float3 N = FaceUVToDir(i, uv);

		float3 irradiance = float3(0.0f, 0.0f, 0.0f);

		float3 up = float3(0.0f, 1.0f, 0.0f);
		float3 right = normalize(cross(up, N));
		up = normalize(cross(N, right));

		float sampleDelta = 0.025f;
		float nrSamples = 0.0f;
		for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
		{
			for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
			{
				// Spherical to Cartesian
				float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
				// Tangent space to world space
				float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

				irradiance += cubeMap.SampleLevel(samplerState, sampleVec, 0).rgb * cos(theta) * sin(theta);
				nrSamples++;
			}
		}

		irradiance /= nrSamples;

		irradianceMap[uint3(tid.xy, i)] = float4(irradiance, 1.0f);
	}
}