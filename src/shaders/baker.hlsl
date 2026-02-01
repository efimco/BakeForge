#include "bvh.hlsl"

cbuffer Constants : register(b0)
{
	float4x4 worldMatrix;
	float4x4 worldMatrixInvTranspose;
	uint2 dimensions;
	float cageOffset;
	uint useSmoothedNormals;
	uint numBLASInstances;
	uint3 _pad;
};

struct BLASInstance
{
	BBox worldBBox;       // 32 bytes
	uint triangleOffset;   // offset into combined triangle buffer
	uint triIndicesOffset; // offset into combined indices buffer
	uint bvhNodeOffset;    // offset into combined BVH nodes buffer
	uint numTriangles;     // number of triangles in this BLAS
};

//those are for actual baking
StructuredBuffer<BLASInstance> gBlasInstances : register(t0);
StructuredBuffer<Tri> gTris : register(t1);
StructuredBuffer<uint> gTrisIndices : register(t2);
StructuredBuffer<BVHNode> gNodes : register(t3);
//ray origin/direction textures
Texture2D<float4> gWorldSpacePositions : register(t4);
Texture2D<float4> gWorldSpaceNormals : register(t5);
Texture2D<float4> gWorldSpaceTangents : register(t6);
Texture2D<float4> gWorldSpaceSmoothedNormals : register(t7);


//this one is for baking output
RWTexture2D<float4> oBakedNormal : register(u0);


float hash(uint2 p) // Hash function for dithering
{
	uint h = p.x * 1597334673u ^ p.y * 3812015801u;
	h = h * 1103515245u + 12345u;
	return float(h) / 4294967295.0f;
}


float3 ditherNoise(uint2 pixel) // Triangular dither noise - better distribution than uniform
{
	float3 noise;
	noise.x = hash(pixel + uint2(0, 0)) + hash(pixel + uint2(1234, 5678)) - 1.0f;
	noise.y = hash(pixel + uint2(4321, 8765)) + hash(pixel + uint2(9999, 1111)) - 1.0f;
	noise.z = hash(pixel + uint2(2468, 1357)) + hash(pixel + uint2(7531, 8642)) - 1.0f;
	return noise;
}


void TestBLASInstances(Ray ray, out uint blasIndex)
{
	blasIndex = 0xFFFFFFFF;
	float tMin = 1e20f;
	for (uint i = 0; i < gBlasInstances.Length; i++)
	{
		BLASInstance instance = gBlasInstances[i];
		float tBox = IntersectBox(ray, instance.worldBBox, tMin);
		if (tBox < tMin)
		{
			blasIndex = i;
			return;
		}
	}
}


#define MAX_STACK_SIZE 64

void TraverseBLAS(Ray ray, BLASInstance inst, inout float bestT, inout float3 bestN)
{
	uint stack[MAX_STACK_SIZE];
	uint stackPtr = 0;
	stack[stackPtr++] = 0; // push root node

	while (stackPtr > 0)
	{
		uint localNodeIdx = stack[--stackPtr];
		uint globalNodeIdx = inst.bvhNodeOffset + localNodeIdx;
		BVHNode node = gNodes[globalNodeIdx];

		// early cull with current best t
		float tBox = IntersectBox(ray, node.bbox, bestT);
		if (tBox >= bestT)
			continue;

		if (node.numTris > 0) // leaf node
		{
			for (uint i = 0; i < node.numTris; i++)
			{
				// firstTriIndex is local to this BLAS's index buffer section
				uint localTriIdx = gTrisIndices[inst.triIndicesOffset + node.firstTriIndex + i];
				// Triangle index is also local, add offset to get global
				Tri tri = gTris[inst.triangleOffset + localTriIdx];
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

			// leftChild is relative within this BLAS
			uint leftLocal = node.leftChild;
			uint rightLocal = leftLocal + 1;

			uint leftGlobal = inst.bvhNodeOffset + leftLocal;
			uint rightGlobal = inst.bvhNodeOffset + rightLocal;

			float tLeft = IntersectBox(ray, gNodes[leftGlobal].bbox, bestT);
			float tRight = IntersectBox(ray, gNodes[rightGlobal].bbox, bestT);

			// Push in far-to-near order so we pop near first
			if (tLeft < tRight)
			{
				if (tRight < bestT) stack[stackPtr++] = rightLocal;
				if (tLeft < bestT) stack[stackPtr++] = leftLocal;
			}
			else
			{
				if (tLeft < bestT) stack[stackPtr++] = leftLocal;
				if (tRight < bestT) stack[stackPtr++] = rightLocal;
			}
		}
	}
}

// Traverse all BLAS instances (two-level acceleration)
void TraverseTLAS(Ray ray, inout float bestT, inout float3 bestN)
{
	for (uint i = 0; i < numBLASInstances; i++)
	{
		BLASInstance inst = gBlasInstances[i];

		// First test instance's world bounding box
		float tBox = IntersectBox(ray, inst.worldBBox, bestT);
		if (tBox >= bestT)
			continue;

		// If hit, traverse this BLAS's BVH
		TraverseBLAS(ray, inst, bestT, bestN);
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
	float3 bestN = float3(0.0f, 0.0f, 0.0f);

	float3 N = normalize(worldNormal.xyz);
	float3 T = normalize(worldTangent.xyz);
	float3 B = cross(N, T);

	// Jitter ray origin in tangent plane (within ~half a texel)
	float3 jitter = ditherNoise(DTid.xy);
	float jitterScale = cageOffset * 0.01f; // 1% of cage offset
	float3 originJitter = (jitter.x * T + jitter.y * B) * jitterScale;

	Ray ray;
	ray.origin = worldPos.xyz + originJitter;
	ray.dir = -normalize(useSmoothedNormals != 0 ? worldSmoothedNormal.xyz : worldNormal.xyz);
	ray.origin -= ray.dir * cageOffset; // offset ray origin back by cage distance
	ray.invDir = 1.0f / ray.dir;

	TraverseTLAS(ray, bestT, bestN);

	float3 tangentSpaceNormal; 	// transforms bestN from world space to tangent space
	tangentSpaceNormal.x = dot(bestN, T);
	tangentSpaceNormal.y = dot(bestN, B);
	tangentSpaceNormal.z = dot(bestN, N);

	tangentSpaceNormal = tangentSpaceNormal * 0.5f + 0.5f; // remap from [-1,1] to [0,1] texture space

	if (bestN.x == 0.0f && bestN.y == 0.0f && bestN.z == 0.0f)
	{
		tangentSpaceNormal = float3(0.5f, 0.5f, 1.0f); // default normal if no intersection
	}

	tangentSpaceNormal = pow(tangentSpaceNormal, float3(2.2f, 2.2f, 2.2f)); // gamma correct

	oBakedNormal[DTid.xy] = float4(tangentSpaceNormal, 1.0f);

}