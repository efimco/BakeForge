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
	float3 normal;
	float pad3;
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
};


// Möller–Trumbore intersection algorithm
bool IntersectTri(Ray ray, Tri tri, out float t)
{
		t = 0.0f;
		float3 edge1 = tri.v1 - tri.v0;
		float3 edge2 = tri.v2 - tri.v0;
		float3 pvec = cross(ray.dir, edge2);
		float det = dot(edge1, pvec);

		if (det == 0.0f)
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

		t = dot(edge2, qvec) * invDet;
		if (t > 0.0f)
		{
			return true;
		}
		return false;
}

bool IntersectBox(Ray ray, BBox box)
{
	// Slab method for ray-AABB intersection
	float3 invDir = 1.0f / ray.dir;
	float3 t0 = (box.min - ray.origin) * invDir;
	float3 t1 = (box.max - ray.origin) * invDir;

	// Swap t0 and t1 if invDir is negative
	float3 tmin = min(t0, t1);
	float3 tmax = max(t0, t1);

	// Find the largest tmin and smallest tmax
	float tNear = max(max(tmin.x, tmin.y), tmin.z);
	float tFar = min(min(tmax.x, tmax.y), tmax.z);

	// Ray intersects box if tNear <= tFar and tFar >= 0
	return tNear <= tFar && tFar >= 0.0f;
}


bool IsLeaf(BVHNode node)
{
	if(node.numTris != 0)
	{
		return true;
	}
	else
	{
		return false;
	}
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

		if (!IntersectBox(ray, node.bbox))
			continue;

		if (IsLeaf(node))
		{
			for (uint i = 0; i < node.numTris; i++)
			{
				float t = 0;
				Tri tri = gTris[gTrisIndices[node.firstTriIndex + i]];
				if (IntersectTri(ray, tri, t))
				{
					if (t < bestT)
					{
						bestT = t;
						bestN = tri.normal;
					}
				}
			}
		}
		else
		{
			// Push children onto stack
			if (stackPtr < MAX_STACK_SIZE - 1)
			{
				stack[stackPtr++] = node.leftChild;
				stack[stackPtr++] = node.leftChild + 1;
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
	ray.origin = mul(worldMatrixInv, float4(rayWorld.origin, 1.0f)).xyz;
	ray.dir = normalize(mul(worldMatrixInv, float4(rayWorld.dir, 0.0f)).xyz);

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
