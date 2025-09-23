cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
	float4x4 model;
	int objectID;
	float3 _pad; // align to 16 bytes
};

Texture2D textureSampler : register(t0);

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
	int objectID : SV_TARGET4;
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
	output.albedo = textureSampler.Sample(samplerState, input.texCoord);
	if (output.albedo.a < 0.1f)
		discard;
	output.metallicRoughness = float2(0.0f, 0.0f);
	output.normal = float4(input.normal, 0.0f);
	output.fragPos = float4(input.fragPos.xyz, 1.0f);
	output.objectID = objectID;
	return output;
}
