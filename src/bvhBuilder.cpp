#pragma
#include "bvhBuilder.hpp"

using namespace BVH;


BVHBuilder::BVHBuilder(const std::vector<Triangle>& triangles,
					   std::vector<uint32_t>& trisIndex_)
	: tris(triangles)
	, trisIndex(trisIndex_)
{
}

void BVHBuilder::SplitNode(uint32_t nodeIndex)
{
	if (nodes[nodeIndex].numTris <= 2)
		return;

	glm::vec3 extent = nodes[nodeIndex].bbox.max - nodes[nodeIndex].bbox.min;

	int splitAxis = 0; // split along x
	if (extent.y > extent.x)
		splitAxis = 1; // split along y
	else if (extent.z > extent[splitAxis])
		splitAxis = 2; // split along z

	// center of the split axis
	float splitPos = nodes[nodeIndex].bbox.min[splitAxis] + extent[splitAxis] * 0.5f;

	int i = nodes[nodeIndex].firstTriIndex;
	int j = nodes[nodeIndex].firstTriIndex + nodes[nodeIndex].numTris - 1;

	while (i <= j)
	{
		if (tris[trisIndex[i]].Center()[splitAxis] < splitPos)
			i++;
		else
			std::swap(trisIndex[i], trisIndex[j--]);
	}

	uint32_t leftCount = i - nodes[nodeIndex].firstTriIndex;
	if (leftCount == 0 || leftCount == nodes[nodeIndex].numTris)
		return;

	uint32_t leftIndex = ++nodesUsed;
	uint32_t rightIndex = ++nodesUsed;
	Node lNode;
	Node rNode;
	nodes.push_back(std::move(lNode));
	nodes.push_back(std::move(rNode));
	nodes[nodeIndex].leftChild = leftIndex;

	nodes[leftIndex].firstTriIndex = nodes[nodeIndex].firstTriIndex;
	nodes[leftIndex].numTris = leftCount;

	nodes[rightIndex].firstTriIndex = i;
	nodes[rightIndex].numTris = nodes[nodeIndex].numTris - leftCount;
	
	nodes[nodeIndex].numTris = 0;

	UpdateBounds(leftIndex);
	UpdateBounds(rightIndex);

	SplitNode(leftIndex);
	SplitNode(rightIndex);
}

void BVHBuilder::UpdateBounds(uint32_t index)
{
	nodes[index].bbox.min = glm::vec3(1e30f);
	nodes[index].bbox.max = glm::vec3(-1e30f);
	for (uint32_t i = 0; i < nodes[index].numTris; i++)
	{
		const uint32_t triIdx = trisIndex[nodes[index].firstTriIndex + i];
		const Triangle& triangle = tris[triIdx];
		float minX = glm::min(triangle.v0.x, glm::min(triangle.v1.x, triangle.v2.x));
		float minY = glm::min(triangle.v0.y, glm::min(triangle.v1.y, triangle.v2.y));
		float minZ = glm::min(triangle.v0.z, glm::min(triangle.v1.z, triangle.v2.z));
		float maxX = glm::max(triangle.v0.x, glm::max(triangle.v1.x, triangle.v2.x));
		float maxY = glm::max(triangle.v0.y, glm::max(triangle.v1.y, triangle.v2.y));
		float maxZ = glm::max(triangle.v0.z, glm::max(triangle.v1.z, triangle.v2.z));
		nodes[index].bbox.min = glm::min(nodes[index].bbox.min, glm::vec3(minX, minY, minZ));
		nodes[index].bbox.max = glm::max(nodes[index].bbox.max, glm::vec3(maxX, maxY, maxZ));
	}
}

std::vector<Node> BVHBuilder::BuildBVH()
{
	Node rootNode;
	rootNode.firstTriIndex = 0;
	rootNode.numTris = static_cast<uint32_t>(tris.size());

	if (rootNode.numTris == 0)
		return nodes;

	nodes.push_back(std::move(rootNode));
	nodesUsed = 0;
	UpdateBounds(nodesUsed);
	SplitNode(nodesUsed);
	return nodes;
}
