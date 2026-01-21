cbuffer CB : register(b0)
{
	matrix viewProjection;
	uint minDepth;
	uint maxDepth;
	uint showLeafsOnly;
	uint padding;
};

struct BBox
{
	float3 min;
	float pad0;
	float3 max;
	float pad1;
};

struct BVHNode
{
	BBox bbox;
	uint leftChild, firstTriIndex, numTris;
	uint depth;
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
	float4 color : COLOR;
};

struct PSOutput
{
	float4 color : SV_TARGET;
};

PSInput VS(VSInput input)
{
	PSInput output;

	BVHNode node = nodes[input.instanceID];

	if (node.depth < minDepth || node.depth > maxDepth)
			output.position = float4(0, 0, 0, 0); // Degenerate
	else if (showLeafsOnly && node.numTris == 0)
	{
		output.position = float4(0, 0, 0, 0);
	}
	else
	{
	float3 center = (node.bbox.min + node.bbox.max) * 0.5;
	float3 extent = (node.bbox.max - node.bbox.min) * 0.5;
	float3 worldPos = center + input.position * extent;
	output.position = mul(float4(worldPos, 1.0), viewProjection);
	}

	// Better color scheme based on node properties
	bool isLeaf = node.numTris > 0;

	if (isLeaf) {
		// Leaf nodes = green
		output.color = float4(0.0, 1.0, 0.0, 0.3);
	} else {
		// Internal nodes = color by depth
		float depthNorm = float(node.depth) / 10.0; // Adjust divisor based on expected max depth
		float3 heatmap = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), saturate(depthNorm));
		output.color = float4(heatmap, 0.2);
	}
	return output;
}

PSOutput PS(PSInput input) : SV_TARGET
{
	PSOutput output;
	output.color = input.color; // Semi-transparent
	return output;
}
