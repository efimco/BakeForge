cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
};

Texture2D textureSampler : register(t0);

SamplerState samplerState :register(s0);

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

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	output.texCoord = input.texCoord;
	output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));
	return output;
}

float4 PS(PixelInputType input) : SV_TARGET
{
	float4 color = textureSampler.Sample(samplerState, input.texCoord);

	return color;
}
