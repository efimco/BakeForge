#include "constants.hlsl"


struct Vertex
{
	float3 position;
	float3 normal;
	float2 uv;
};

struct Tri
{
	float3 v0;
	float pad0;
	float3 v1;
	float pad1;
	float3 v2;
	float pad2;
	float3 n0;  // vertex normal at v0
	float pad3;
	float3 n1;  // vertex normal at v1
	float pad4;
	float3 n2;  // vertex normal at v2
	float pad5;
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

StructuredBuffer<Tri> gTris : register(t0);
StructuredBuffer<uint> gTrisIndices : register(t1);
StructuredBuffer<BVHNode> gNodes : register(t2);

cbuffer cb : register(b0)
{
	uint2 demensions;
	float2 padding;

	float3 camPosition;
	float padding2;

	float3 camForward;
	float padding3;

	float3 camRight;
	float padding4;

	float3 camUp;
	float camFOV;

	float4x4 worldMatrix;
	float4x4 worldMatrixInv;
}


float deg2Rad(float deg)
{
	float rad = deg * (PI / 180.0f);
	return rad;
}

struct Ray
{
	float3 origin;
	float3 dir;
	float3 invDir;
};


// Möller–Trumbore intersection algorithm - outputs barycentric coords for normal interpolation
bool IntersectTri(Ray ray, Tri tri, inout float t, out float2 baryOut)
{
	baryOut = float2(0, 0);
	float3 edge1 = tri.v1 - tri.v0;
	float3 edge2 = tri.v2 - tri.v0;
	float3 pvec = cross(ray.dir, edge2);
	float det = dot(edge1, pvec);

	// Use small epsilon for near-parallel rays
	if (abs(det) < 1e-8f)
		return false;

	float invDet = 1.0f / det;
	float3 tvec = ray.origin - tri.v0;
	float u = dot(tvec, pvec) * invDet;
	if (u < 0.0f || u > 1.0f)
		return false;

	float3 qvec = cross(tvec, edge1);
	float v = dot(ray.dir, qvec) * invDet;
	if (v < 0.0f || u + v > 1.0f)
		return false;

	float hitT = dot(edge2, qvec) * invDet;
	if (hitT > 0.0f && hitT < t)
	{
		t = hitT;
		baryOut = float2(u, v);
		return true;
	}
	return false;
}

// Optimized slab method - returns tNear for ordering, uses precomputed invDir
float IntersectBox(Ray ray, BBox box, float tMax)
{
	float3 t0 = (box.min - ray.origin) * ray.invDir;
	float3 t1 = (box.max - ray.origin) * ray.invDir;

	float3 tmin = min(t0, t1);
	float3 tmax = max(t0, t1);

	float tNear = max(max(tmin.x, tmin.y), tmin.z);
	float tFar = min(min(tmax.x, tmax.y), tmax.z);

	// Return tNear if hit, otherwise return large value
	return (tNear <= tFar && tFar >= 0.0f && tNear < tMax) ? tNear : 1e30f;
}


bool IsLeaf(BVHNode node)
{
	return node.numTris != 0;
}

#define MAX_STACK_SIZE 64

void TraverseBVH(Ray ray, inout float bestT, inout float3 bestN)
{
	uint stack[MAX_STACK_SIZE];
	int stackPtr = 0;
	stack[stackPtr++] = 0; // Start with root node

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
			// Get both children
			uint leftIdx = node.leftChild;
			uint rightIdx = leftIdx + 1;

			// Test both children
			float tLeft = IntersectBox(ray, gNodes[leftIdx].bbox, bestT);
			float tRight = IntersectBox(ray, gNodes[rightIdx].bbox, bestT);

			// Push far child first (so near child is processed first)
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


RWTexture2D<float4> outputColor : register(u0);

[numthreads(16, 16, 1)]
void CS(uint2 tid : SV_DispatchThreadID)
{
	if (tid.x >= demensions.x || tid.y >= demensions.y)
		return;

	float aspect = float(demensions.x) / float(demensions.y);

	// Convert pixel to NDC [-1, 1]
	// In DirectX, screen Y=0 is top, but NDC Y=+1 is top
	float2 uv = (float2(tid) + 0.5f) / float2(demensions);
	float2 ndc;
	ndc.x = uv.x * 2.0f - 1.0f;       // Left=-1, Right=+1
	ndc.y = (1.0f - uv.y) * 2.0f - 1.0f; // Top=+1, Bottom=-1

	float fovY = deg2Rad(camFOV);
	float tanHalfFovY = tan(fovY * 0.5f);

	// Vertical FOV: Y uses tan(fov/2), X uses tan(fov/2) * aspect
	float2 film;
	film.y = ndc.y * tanHalfFovY;
	film.x = ndc.x * tanHalfFovY * aspect;

	// World space ray
	Ray rayWorld;
	rayWorld.origin = camPosition;
	rayWorld.dir = normalize(camForward + film.x * camRight + film.y * camUp);

	// Transform ray to object space (GLM is column-major, use mul(matrix, vector))
	Ray ray;
	ray.origin = camPosition;
	ray.dir = normalize(mul(worldMatrixInv, float4(rayWorld.dir, 0.0f)).xyz);
	ray.invDir = 1.0f / ray.dir;

	float bestT = 1e30f;
	float3 bestN = 0.0f;
	TraverseBVH(ray, bestT, bestN);
	float3 color = float3(0,0,0);

	if (bestT < 1e29f)
	{
		// Transform normal back to world space (use transpose of inverse = adjugate for normals)
		bestN = normalize(mul((float3x3)transpose(worldMatrixInv), bestN));
		// simple visualization: normal -> color
		color = 0.5f * (bestN + 1.0f);

		// optional: distance fade
		// color *= 1.0f / (1.0f + 0.1f * bestT);
	}

	outputColor[tid] = float4(color, 1.0f);
}
