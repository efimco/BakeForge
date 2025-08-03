cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
};

Texture2D textureSampler : register(t0);

SamplerState samplerState : register(s0);

struct VertexInputType
{
	float3 position : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};


struct PSOutput
{
	float albedo : SV_TARGET0;
	float2 metallicRoughness : SV_TARGET1;
	float4 normal : SV_TARGET2;
	float4 position : SV_TARGET3;
}

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	output.texCoord = input.texCoord;
	output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));
	return output;
}

PSOutput PS(PixelInputType input) : SV_TARGET
{
	PSOutput output;
	output.albedo = textureSampler.Sample(samplerState, input.texCoord);
	output.metallicRoughness = float2(0.0f, 0.0f);
	output.normal = float4(input.normal, 0.0f);
	output.position = float4(input.position.xyz, 1.0f);
	return output;
}
