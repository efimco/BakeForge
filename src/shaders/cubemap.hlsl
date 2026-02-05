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
	float4x4 viewProj;
	float mapRotationY;
	uint isBlurred;
	float blurAmount;
	float backgroundIntensity;
	float maxMipLevel; // Pass the max mip level from CPU
};

static const float YAW = PI / 180.0;
static float3 MOD3 = float3(443.8975, 397.2973, 491.1871);

// Dithering to reduce banding
float3 hash32(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * MOD3);
	p3 += dot(p3, p3.yxz + 19.19);
	return frac(float3((p3.x + p3.y) * p3.z, (p3.x + p3.z) * p3.y, (p3.y + p3.z) * p3.x));
}

void applyDitheredNoise(float2 screenPos, inout float4 color)
{
	float3 rnd = hash32(screenPos);
	color.rgb += (rnd - 0.5) /  512.0;
}

TextureCube prefilteredMap : register(t0);
Texture2D  hdrEquirectMap : register(t1);

SamplerState samplerState : register(s0);

// Smooth blur sampling with multiple samples for better quality
float3 sampleBlurredHDRI(float2 uv, float lod)
{
	// Get texture dimensions for proper offset calculation
	uint width, height, mipLevels;
	hdrEquirectMap.GetDimensions(0, width, height, mipLevels);

	// Calculate texel size at this mip level
	float mipScale = pow(2.0, lod);
	float2 texelSize = mipScale / float2(width, height);

	// Multi-sample blur pattern (rotated grid for better coverage)
	// Using 5 samples: center + 4 diagonal offsets
	const float spreadFactor = 0.5; // Adjust for blur spread
	float2 offset = texelSize * spreadFactor;

	float3 color = float3(0, 0, 0);

	// Center sample (highest weight)
	color += hdrEquirectMap.SampleLevel(samplerState, uv, lod).rgb * 0.4;

	// Diagonal samples with rotation for better coverage
	color += hdrEquirectMap.SampleLevel(samplerState, uv + float2(offset.x, offset.y), lod).rgb * 0.15;
	color += hdrEquirectMap.SampleLevel(samplerState, uv + float2(-offset.x, offset.y), lod).rgb * 0.15;
	color += hdrEquirectMap.SampleLevel(samplerState, uv + float2(offset.x, -offset.y), lod).rgb * 0.15;
	color += hdrEquirectMap.SampleLevel(samplerState, uv + float2(-offset.x, -offset.y), lod).rgb * 0.15;

	return color;
}

VertexOutput VS(CubeMapVertex input)
{
	VertexOutput output;
	output.localPos = input.position;
	float4 pos = mul(float4(input.position, 1.0f), viewProj);
	output.position = pos.xyww; // Set z = w to ensure depth is 1.0 (far plane)

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

	// Convert direction to spherical coordinates for equirectangular sampling
	float phi = atan2(direction.z, direction.x);
	float theta = acos(direction.y);

	// Map to UV coordinates [0,1]
	float2 uv;
	uv.x = phi / (TwoPI)+0.5;
	uv.y = theta / PI;

	if (isBlurred)
	{
		// blurAmount 0-1 maps to LOD 0 to LOD 5 with smooth curve
		float t = blurAmount;
		float lod = t * 5;  // max MipLevel is 5 
		// Use multi-sample blur for smoother results
		float3 prefilteredColor = sampleBlurredHDRI(uv, lod);
		prefilteredColor *= backgroundIntensity;
		float4 result = float4(prefilteredColor, 1.0f);
		applyDitheredNoise(input.position.xy, result);
		return result;
	}

	float3 hdrColor = hdrEquirectMap.SampleLevel(samplerState, uv, 0).rgb;
	hdrColor *= backgroundIntensity;
	float4 result = float4(hdrColor, 1.0f);
	applyDitheredNoise(input.position.xy, result);
	return result;
}