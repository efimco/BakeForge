#include "bvh.hlsl"

cbuffer Constants : register(b0)
{
	float4x4 worldMatrix;
	float4x4 worldMatrixInvTranspose;
	uint2 dimensions;
	float cageOffset;
};

struct BarycentricVertex
{
	float3 position;
	float3 normal;
	float2 texCoords;
};

struct BarycentricTri
{
	BarycentricVertex v0;
	BarycentricVertex v1;
	BarycentricVertex v2;
};



//those are for making ray origin/direction textures
StructuredBuffer<BarycentricVertex> gVerticies : register(t0);
StructuredBuffer<uint> gIndices : register(t1);

//those are for actual baking
StructuredBuffer<Tri> gTris : register(t2);
StructuredBuffer<uint> gTrisIndices : register(t3);
StructuredBuffer<BVHNode> gNodes : register(t4);
Texture2D<float4> gWorldSpacePositions : register(t5);
Texture2D<float4> gWorldSpaceNormals : register(t6);

//ray origin/direction textures
RWTexture2D<float4> oWorldSpaceTexelPosition : register(u0);
RWTexture2D<float4> oWorldSpaceTexelNormal : register(u1);

//this one is for baking output
RWTexture2D<float4> oBakedNormal : register(u2);

bool uvInsideTriangle(float2 uv, BarycentricTri tri, out float3 barycentric)
{
	float2 v0 = tri.v1.texCoords - tri.v0.texCoords;
	float2 v1 = tri.v2.texCoords - tri.v0.texCoords;
	float2 v2 = uv - tri.v0.texCoords;

	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;

	barycentric.y = (d11 * d20 - d01 * d21) / denom;
	barycentric.z = (d00 * d21 - d01 * d20) / denom;
	barycentric.x = 1.0f - barycentric.y - barycentric.z;

	return (barycentric.x >= 0) && (barycentric.y >= 0) && (barycentric.z >= 0);
}

#define MAX_STACK_SIZE 32

void TraverseBVH(Ray ray, inout float bestT, inout float3 bestN)
{
	uint stack[MAX_STACK_SIZE];
	uint stackPtr = 0;
	stack[stackPtr++] = 0; // push root node

	while (stackPtr > 0)
	{
		uint nodeIdx = stack[--stackPtr];
		BVHNode node = gNodes[nodeIdx];

		// Early cull with current best t
		float tBox = IntersectBox(ray, node.bbox, bestT);
		if (tBox >= bestT)
			continue;

		if (node.numTris > 0) // Leaf node
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
void CSPrepareTextures(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= dimensions.x || DTid.y >= dimensions.y)
		return;

	float2 uv = (float2(DTid.xy) + 0.5f) / dimensions;

	oWorldSpaceTexelPosition[DTid.xy] = float4(0.0, 0.0, 0.0f, 0.0f);
	oWorldSpaceTexelNormal[DTid.xy] = float4(0.0, 0.0, 0.0f, 0.0f);

	for (uint i = 0; i < gIndices.Length; i += 3)
	{
		BarycentricTri tri;
		tri.v0 = gVerticies[gIndices[i + 0]];
		tri.v1 = gVerticies[gIndices[i + 1]];
		tri.v2 = gVerticies[gIndices[i + 2]];
		float3 barycentric;
		if (uvInsideTriangle(uv, tri, barycentric))
		{
			float3 localPos = barycentric.x * tri.v0.position
				+ barycentric.y * tri.v1.position
				+ barycentric.z * tri.v2.position;

			float3 localNormal = normalize(
				barycentric.x * tri.v0.normal
				+ barycentric.y * tri.v1.normal
				+ barycentric.z * tri.v2.normal);

			float3 worldPos = mul(float4(localPos, 1.0f), worldMatrix).xyz;
			float3 worldNormal = mul(float4(localNormal, 1.0f), worldMatrixInvTranspose).xyz;
			oWorldSpaceTexelPosition[DTid.xy] = float4(worldPos, 1.0f);
			oWorldSpaceTexelNormal[DTid.xy] = float4(worldNormal, 1.0f);
			break;
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

	if (worldPos.a == 0.0f) // we write alpha = 0 for invalid texels
		return;

	Ray ray;
	ray.origin = worldPos.xyz;
	ray.dir = -normalize(worldNormal.xyz);
	ray.origin -= ray.dir * cageOffset; // offset to avoid self-intersection
	ray.invDir = 1.0f / ray.dir;
	float bestT = cageOffset * 6.0f;
	float3 bestN = 0.0f;
	TraverseBVH(ray, bestT, bestN);

	float3 N = normalize(worldNormal.xyz);
	float3 T = normalize(cross(N, abs(N.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0)));
	float3 B = cross(N, T); // build tangent frame from the low-poly normal


	float3 tangentSpaceNormal; 	// transforms bestN from world space to tangent space
	tangentSpaceNormal.x = dot(bestN, T);
	tangentSpaceNormal.y = dot(bestN, B);
	tangentSpaceNormal.z = dot(bestN, N);


	oBakedNormal[DTid.xy] = float4(tangentSpaceNormal * 0.5f + 0.5f, 1.0f); // remap from [-1,1] to [0,1] for storage

}