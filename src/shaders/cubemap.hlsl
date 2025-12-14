#include "constants.hlsl"

struct CubeMapVertex
{
	float3 position : POSITION;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float3 localPos : TEXCOORD0;
};

cbuffer CubeMapCB : register(b0)
{
	float4x4 view;
	float mapRotationY;
	bool isBlurred;
	float blurAmount;
	float2 padding; // align to 16 bytes
};

static const float YAW = PI / 180.0;

TextureCube prefilteredMap : register(t0);
Texture2D  hdrEquirectMap : register(t1);

SamplerState samplerState : register(s0);

VertexOutput VS(CubeMapVertex input)
{
	VertexOutput output;
	output.localPos = input.position;

	float4x4 viewNoTranslation = view;
	viewNoTranslation[3][0] = 0.0;
	viewNoTranslation[3][1] = 0.0;
	viewNoTranslation[3][2] = 0.0;
	viewNoTranslation[3][3] = 1.0;

	float scale = 10.0; // Adjust this value to make the cube bigger
	float4 pos = mul(float4(input.position * scale, 1.0f), viewNoTranslation);
	output.position = pos.xyww; // Set w = z to ensure depth is 1.0

	return output;
}

float3x3 getMapRotation()
{
	float angle = mapRotationY * YAW;
	float cosA = cos(angle);
	float sinA = sin(angle);
	return float3x3(cosA, 0.0, -sinA, 0.0, 1.0, 0.0, sinA, 0.0, cosA);
}

float4 PS(VertexOutput input) : SV_TARGET
{
	float3x3 rotationMatrix = getMapRotation();
	float3 direction = mul(rotationMatrix, input.localPos);
	direction = normalize(direction);

	if (isBlurred)
	{
		// Sample prefiltered cubemap for blurred effect
		// The LOD level is determined by the blurAmount
		float lod = blurAmount; // You can scale this value as needed
		float3 prefilteredColor = prefilteredMap.SampleLevel(samplerState, direction, lod).rgb;
		return float4(prefilteredColor, 1.0f);
	}
	// Convert direction to spherical coordinates for equirectangular sampling
	float phi = atan2(direction.z, direction.x);
	float theta = acos(direction.y);

	// Map to UV coordinates [0,1]
	float2 uv;
	uv.x = phi / (TwoPI)+0.5;
	uv.y = theta / PI;

	// Sample HDR equirectangular map
	float3 hdrColor = hdrEquirectMap.Sample(samplerState, uv).rgb;
	return float4(hdrColor, 1.0f);
}