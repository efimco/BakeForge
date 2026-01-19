cbuffer CB : register(b0)
{
	matrix viewProjection;
};

struct BBox
{
	float3 min;
	float3 max;
};
struct BVHNode
{
	BBox bbox;
	uint leftChild, firstTriIndex, numTris;
};

StructuredBuffer<BVHNode> nodes: register(t0);

struct VSInput
{
	float3 position : POSITION;
	uint instanceID : SV_InstanceID;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float3 color : COLOR;
};

struct PSOutput
{
	float4 color : SV_TARGET;
};

PSInput VS(VSInput input)
{
	PSInput output;

	BVHNode node = nodes[input.instanceID];

	float3 center = (node.bbox.min + node.bbox.max) * 0.5;
	float3 extent = (node.bbox.max - node.bbox.min) * 0.5;
	float3 worldPos = center + input.position * extent;



	output.position = mul(float4(worldPos, 1.0), viewProjection);

	// Color based on depth (optional visualization)
	float depth = float(input.instanceID) / 10.0;
	output.color = float3(1.0, 1.0 - depth, 1.0 - depth);
	return output;
}

PSOutput PS(PSInput input) : SV_TARGET
{
	PSOutput output;
	output.color = float4(input.color, 0.3); // Semi-transparent
	return output;
}
