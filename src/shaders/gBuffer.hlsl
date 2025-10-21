cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
	float4x4 model;
	int objectID;
	bool isSelected;
	float2 _pad; // align to 16 bytes
};

Texture2D albedoTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D normalTexture : register(t2);

SamplerState samplerState : register(s0);

struct VertexInput
{
	float3 position : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
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
	uint objectID : SV_TARGET4;
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

PSOutput PS(VertexOutput input)
{
	PSOutput output;
	output.albedo = albedoTexture.Sample(samplerState, input.texCoord);
	if (output.albedo.a < 0.1f)
	{
		discard;
	}
	if (isSelected)
	{
		output.albedo = output.albedo * float4(1.0f, 0.5f, 0.5f, 1.0f);
	}
	float roughness = metallicRoughnessTexture.Sample(samplerState, input.texCoord).g;
	float metallic = metallicRoughnessTexture.Sample(samplerState, input.texCoord).b;
	output.metallicRoughness = float2(metallic, roughness);
	output.normal = float4(normalize(input.normal * 2.0f - 1.0f), 1.0f);
	output.fragPos = float4(input.fragPos.xyz, 1.0f);
	output.objectID = objectID;
	return output;
}
