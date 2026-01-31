#include "bvh.hlsl"

cbuffer Constants : register(b0)
{
	float4x4 worldMatrix;
	float4x4 worldMatrixInvTranspose;
	uint2 dimensions;
	float cageOffset;
	uint useSmoothedNormals;
};


//those are for actual baking
StructuredBuffer<Tri> gTris : register(t0);
StructuredBuffer<uint> gTrisIndices : register(t1);
StructuredBuffer<BVHNode> gNodes : register(t2);
//ray origin/direction textures
Texture2D<float4> gWorldSpacePositions : register(t3);
Texture2D<float4> gWorldSpaceNormals : register(t4);
Texture2D<float4> gWorldSpaceTangents : register(t5);
Texture2D<float4> gWorldSpaceSmoothedNormals : register(t6);


//this one is for baking output
RWTexture2D<float4> oBakedNormal : register(u0);


#define MAX_STACK_SIZE 64

void TraverseBVH(Ray ray, inout float bestT, inout float3 bestN)
{
	uint stack[MAX_STACK_SIZE];
	uint stackPtr = 0;
	stack[stackPtr++] = 0; // push root node

	while (stackPtr > 0)
	{
		uint nodeIdx = stack[--stackPtr];
		BVHNode node = gNodes[nodeIdx];

		// early cull with current best t
		float tBox = IntersectBox(ray, node.bbox, bestT);
		if (tBox >= bestT)
			continue;

		if (node.numTris > 0) // leaf node
		{
			for (uint i = 0; i < node.numTris; i++)
			{
				uint triIdx = gTrisIndices[node.firstTriIndex + i];
				Tri tri = gTris[triIdx];
				float2 bary;
				if (IntersectTri(ray, tri, bestT, bary))
				{
					// Interpolate vertex normals using barycentric coordinates
					// bary.x = u (weight for v1), bary.y = v (weight for v2), (1-u-v) = weight for v0
					bestN = normalize((1.0f - bary.x - bary.y) * tri.n0 + bary.x * tri.n1 + bary.y * tri.n2);
				}
			}
		}
		else
		{

			uint leftIdx = node.leftChild;
			uint rightIdx = leftIdx + 1;

			float tLeft = IntersectBox(ray, gNodes[leftIdx].bbox, bestT);
			float tRight = IntersectBox(ray, gNodes[rightIdx].bbox, bestT);

			if (tLeft < tRight)
			{
				if (tRight < bestT) stack[stackPtr++] = rightIdx;
				if (tLeft < bestT) stack[stackPtr++] = leftIdx;
			}
			else
			{
				if (tLeft < bestT) stack[stackPtr++] = leftIdx;
				if (tRight < bestT) stack[stackPtr++] = rightIdx;
			}
		}
	}
}

[numthreads(16, 16, 1)]
void CSBakeNormal(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= dimensions.x || DTid.y >= dimensions.y)
		return;

	float4 worldPos = gWorldSpacePositions.Load(int3(DTid.xy, 0));
	float4 worldNormal = gWorldSpaceNormals.Load(int3(DTid.xy, 0));
	float4 worldTangent = gWorldSpaceTangents.Load(int3(DTid.xy, 0));
	float4 worldSmoothedNormal = gWorldSpaceSmoothedNormals.Load(int3(DTid.xy, 0));

	float bestT = 1e20f;
	float3 bestN = float3(0.5, 0.5f, 1.0f);

	if (worldPos.a == 0.0f) // i clear alpha = 0 for invalid texels
	{
		bestN = pow(bestN, float3(2.2f, 2.2f, 2.2f)); // gamma correct
		oBakedNormal[DTid.xy] = float4(bestN, 1.0f);
		return;
	}


	Ray ray;
	ray.origin = worldPos.xyz;
	ray.dir = -normalize(useSmoothedNormals != 0 ? worldSmoothedNormal.xyz : worldNormal.xyz);
	ray.origin -= ray.dir * cageOffset; // offset to avoid self-intersection
	ray.invDir = 1.0f / ray.dir;

	TraverseBVH(ray, bestT, bestN);
	float3 N = normalize(worldNormal.xyz);
	float3 T = normalize(worldTangent.xyz);
	float3 B = cross(N, T); // build tangent frame from the low-poly normal


	float3 tangentSpaceNormal; 	// transforms bestN from world space to tangent space
	tangentSpaceNormal.x = dot(bestN, T);
	tangentSpaceNormal.y = dot(bestN, B);
	tangentSpaceNormal.z = dot(bestN, N);

	tangentSpaceNormal = tangentSpaceNormal * 0.5f + 0.5f; // remap from [-1,1] to [0,1] for storage
	tangentSpaceNormal = pow(tangentSpaceNormal, float3(2.2f, 2.2f, 2.2f)); // gamma correct

	oBakedNormal[DTid.xy] = float4(tangentSpaceNormal, 1.0f); // remap from [-1,1] to [0,1] for storage

}