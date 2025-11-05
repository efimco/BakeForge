#include "constants.hlsl"

Texture2D hdrEquirect : register(t0);
SamplerState samplerState : register(s0);
RWTexture2DArray<float4> gCubeUAV : register(u0);

cbuffer CB : register(b0)
{
	uint gFaceSize;     // cube width=height per face
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

float2 DirToEquirectUV(float3 dir)
{
	// dir is normalized
	float phi = atan2(dir.z, dir.x);        // [-pi, pi]
	float theta = asin(clamp(dir.y, -1, 1));  // [-pi/2, pi/2]
	float2 uv;
	uv.x = 0.5f + phi * (1.0f / (2.0f * PI));
	uv.y = 0.5f - theta * (1.0f / PI);
	return uv;
}



[numthreads(8, 8, 1)]
void CS(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= gFaceSize || tid.y >= gFaceSize || tid.z >= 6) return;

	// Map pixel center to uv in [-1,1]
	float2 st = (float2(tid.xy) + 0.5f) / gFaceSize; // [0,1]
	float2 uv = 2.0f * st - 1.0f;                    // [-1,1]

	// Note: sampling along cube face needs to be exactly square (no aspect skew)
	float3 dir = FaceUVToDir(tid.z, uv);

	float2 eq = DirToEquirectUV(dir);
	float4 hdr = hdrEquirect.SampleLevel(samplerState, eq, 0); // sample base mip

	gCubeUAV[uint3(tid.xy, tid.z)] = hdr;
}
