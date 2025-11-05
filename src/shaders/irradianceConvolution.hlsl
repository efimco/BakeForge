TextureCube cubeMap : register(t0);
SamplerState samplerState : register(s0);
RWTexture2DArray<float4> irradianceMap : register(u0);

cbuffer CB : register(b0)
{
	uint gCubeFaceSize;
	uint3 _pad;
}
static const uint SAMPLE_COUNT = 1024;
static const float PI = 3.14159265359f;

// Generate better pseudo-random numbers
float rand(float2 co)
{
	return frac(sin(dot(co.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// Build tangent space from normal
void buildTangentSpace(float3 N, out float3 T, out float3 B)
{
	float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
	T = normalize(cross(up, N));
	B = cross(N, T);
}

// Convert from tangent space to world space
float3 tangentToWorld(float3 vec, float3 N, float3 T, float3 B)
{
	return vec.x * T + vec.y * B + vec.z * N;
}

float3 ImportanceSampleLambertian(float2 xi)
{
	// Cosine-weighted hemisphere sampling
	float phi = 2.0f * PI * xi.x;
	float cosTheta = sqrt(1.0f - xi.y);
	float sinTheta = sqrt(xi.y);

	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	return H;
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

	// Map pixel center to uv in [-1,1]
	float2 st = (float2(tid.xy) + 0.5f) / float(gCubeFaceSize); // [0,1]
	float2 uv = 2.0f * st - 1.0f;                              // [-1,1]

	// Convert (face, uv) to direction for each face
	float3 directions[6];

	directions[0] = FaceUVToDir(0, uv);
	directions[1] = FaceUVToDir(1, uv);
	directions[2] = FaceUVToDir(2, uv);
	directions[3] = FaceUVToDir(3, uv);
	directions[4] = FaceUVToDir(4, uv);
	directions[5] = FaceUVToDir(5, uv);

	// Process all 6 faces
	for (uint face = 0; face < 6; ++face)
	{
		float3 N = directions[face];

		// Build tangent space for this normal
		float3 T, B;
		buildTangentSpace(N, T, B);

		float3 irradiance = float3(0.0f, 0.0f, 0.0f);

		// Monte Carlo integration
		for (uint i = 0; i < SAMPLE_COUNT; ++i)
		{
			// Generate better random samples
			float2 xi = float2(
				rand(float2(tid.x + i * 37, tid.y + face * 13)),
				rand(float2(tid.y + i * 73, tid.x + face * 17))
			);

			// Get sample direction in tangent space
			float3 sampleTangent = ImportanceSampleLambertian(xi);

			// Transform to world space
			float3 L = tangentToWorld(sampleTangent, N, T, B);

			float NdotL = max(dot(N, L), 0.0f);
			if (NdotL > 0.0f)
			{
				// Sample the environment map using the transformed direction
				float4 sampleColor = cubeMap.SampleLevel(samplerState, L, 0);
				irradiance += sampleColor.rgb * NdotL;
			}
		}

		// Apply proper scaling for Lambert diffuse BRDF
		irradiance = irradiance * (PI / float(SAMPLE_COUNT));

		// Write to the correct face of the texture array
		irradianceMap[uint3(tid.xy, face)] = float4(irradiance, 1.0f);
	}
}