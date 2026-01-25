#include "BRDF.hlsl"

cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
	float4x4 model;
	float3 cameraPosition;
	float objectID;
	float4 albedoColor;
	float metallicValue;
	float roughnessValue;
	float2 padding;
};

Texture2D albedoTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D normalTexture : register(t2);

SamplerState samplerState : register(s0);

struct VertexInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 fragPos : NORMAL1;
};


struct PSOutput
{
	float4 albedo : SV_TARGET0;
	float2 metallicRoughness : SV_TARGET1;
	float4 normal : SV_TARGET2;
	float4 fragPos : SV_TARGET3;
	float objectID : SV_TARGET4;
};

VertexOutput VS(VertexInput input)
{
	VertexOutput output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	output.fragPos = mul(float4(input.position, 1.0f), model);
	output.texCoord = input.texCoord;
	output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));
	return output;
}

float3 getNormalFromMap(VertexOutput input, bool hasNormalMap = true)
{
	float3 tangentNormal = normalTexture.Sample(samplerState, input.texCoord).xyz * 2.0f - 1.0f;
	if (!hasNormalMap)
		return input.normal;
	// If no normal map (flat normal), use geometry normal
	if (length(tangentNormal) < 0.01f)
		return input.normal;
	// Invert Y for DirectX normal maps
	tangentNormal.y = -tangentNormal.y;
	float3 Q1 = ddx(input.fragPos.xyz);
	float3 Q2 = ddy(input.fragPos.xyz);
	float2 st1 = ddx(input.texCoord);
	float2 st2 = ddy(input.texCoord);

	float3 N = normalize(input.normal);
	float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
	T.y = -T.y;
	float3 B = normalize(cross(N, T));
	float3x3 TBN = float3x3(T, B, N);

	return normalize(mul(tangentNormal, TBN));
}

PSOutput PS(VertexOutput input)
{
	uint albedoWidth, albedoHeight;
	albedoTexture.GetDimensions(albedoWidth, albedoHeight);
	bool hasAlbedoTexture = albedoWidth != 0 && albedoHeight != 0;

	uint metallicRoughnessWidth, metallicRoughnessHeight;
	metallicRoughnessTexture.GetDimensions(metallicRoughnessWidth, metallicRoughnessHeight);
	bool hasMetallicRoughnessTexture = metallicRoughnessWidth != 0 && metallicRoughnessHeight != 0;

	uint normalWidth, normalHeight;
	normalTexture.GetDimensions(normalWidth, normalHeight);
	bool hasNormalTexture = normalWidth != 0 && normalHeight != 0;

	PSOutput output;
	output.albedo = hasAlbedoTexture ? albedoTexture.Sample(samplerState, input.texCoord) : albedoColor;

	output.fragPos = float4(input.fragPos.xyz, 1.0f);
	float3 worldNormal = getNormalFromMap(input, hasNormalTexture);
	float3 N = worldNormal;
	float3 V = normalize(cameraPosition - input.fragPos.xyz);
	float NdotV = dot(N, V);
	float3 selectionColor = float3(1.9, 0.5, 0.0);
	if (output.albedo.a < 0.1f)
	{
		discard;
	}
	float gammaFactor = 2.6f;
	output.albedo = float4(pow(output.albedo.rgb, float3(gammaFactor, gammaFactor, gammaFactor)), output.albedo.a);
	float roughness = hasMetallicRoughnessTexture ? metallicRoughnessTexture.Sample(samplerState, input.texCoord).g : roughnessValue;
	float metallic = hasMetallicRoughnessTexture ? metallicRoughnessTexture.Sample(samplerState, input.texCoord).b : metallicValue;
	output.metallicRoughness = float2(metallic, roughness);

	// Encode normal from [-1,1] to [0,1] for storage in UNORM render target
	output.normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);


	output.objectID = objectID;
	return output;
}
