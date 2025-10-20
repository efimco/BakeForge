cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
};

struct VertexInput
{
	float3 position : POSITION;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
};


VertexOutput VS(VertexInput input)
{
	VertexOutput output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	return output;
}

