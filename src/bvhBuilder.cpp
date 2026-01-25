#pragma
#include "bvhBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

using namespace BVH;


BVHBuilder::BVHBuilder(const std::vector<Triangle>& triangles,
					   std::vector<uint32_t>& trisIndex_)
	: tris(triangles)
	, trisIndex(trisIndex_)
{
}

void BVHBuilder::SplitNode(uint32_t nodeIndex, uint32_t depth)
{
	nodes[nodeIndex].depth = depth;

	if (nodes[nodeIndex].numTris <= 2)
		return;

	glm::vec3 extent = nodes[nodeIndex].bbox.max - nodes[nodeIndex].bbox.min;

	int splitAxis = 0; // split along x
	if (extent.y > extent[splitAxis])
		splitAxis = 1; // split along y
	if (extent.z > extent[splitAxis])
		splitAxis = 2; // split along z

	// center of the split axis
	float splitPos = nodes[nodeIndex].bbox.min[splitAxis] + extent[splitAxis] * 0.5f;

	int firstTriIndex = nodes[nodeIndex].firstTriIndex;
	int lastTriIndex = nodes[nodeIndex].firstTriIndex + nodes[nodeIndex].numTris - 1;

	while (firstTriIndex <= lastTriIndex)
	{
		if (tris[trisIndex[firstTriIndex]].Center()[splitAxis] < splitPos)
			firstTriIndex++;
		else
			std::swap(trisIndex[firstTriIndex], trisIndex[lastTriIndex--]);
	}

	uint32_t leftCount = firstTriIndex - nodes[nodeIndex].firstTriIndex;
	
	// Fallback to median split if midpoint split failed (all tris on one side)
	if (leftCount == 0 || leftCount == nodes[nodeIndex].numTris)
	{
		// Sort triangles by center along split axis and split at median
		uint32_t first = nodes[nodeIndex].firstTriIndex;
		uint32_t count = nodes[nodeIndex].numTris;
		
		std::sort(trisIndex.begin() + first, trisIndex.begin() + first + count,
			[this, splitAxis](uint32_t a, uint32_t b)
			{
				return tris[a].Center()[splitAxis] < tris[b].Center()[splitAxis];
			});
		
		leftCount = count / 2;
		firstTriIndex = first + leftCount;
	}

	uint32_t leftIndex = ++nodesUsed;
	uint32_t rightIndex = ++nodesUsed;
	Node lNode{};
	Node rNode{};
	nodes.push_back(lNode);
	nodes.push_back(rNode);
	nodes[nodeIndex].leftChild = leftIndex;

	nodes[leftIndex].firstTriIndex = nodes[nodeIndex].firstTriIndex;
	nodes[leftIndex].numTris = leftCount;

	nodes[rightIndex].firstTriIndex = firstTriIndex;
	nodes[rightIndex].numTris = nodes[nodeIndex].numTris - leftCount;
	
	nodes[nodeIndex].numTris = 0;

	UpdateBounds(leftIndex);
	UpdateBounds(rightIndex);

	SplitNode(leftIndex, depth + 1);
	SplitNode(rightIndex, depth + 1);
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

BVHStats BVHBuilder::CalculateStats(const std::vector<Node>& nodes)
{
	BVHStats stats;
	if (nodes.empty())
		return stats;

	stats.totalNodes = static_cast<uint32_t>(nodes.size());
	uint64_t depthSum = 0;

	std::function<void(uint32_t, uint32_t)> traverse = [&](uint32_t idx, uint32_t depth)
	{
		if (idx >= nodes.size())
			return;

		const auto& node = nodes[idx];
		stats.maxDepth = std::max(stats.maxDepth, depth);

		if (node.numTris > 0) // Leaf node
		{
			stats.leafNodes++;
			stats.totalTriangles += node.numTris;
			stats.maxTrisPerLeaf = std::max(stats.maxTrisPerLeaf, node.numTris);
			stats.minTrisPerLeaf = std::min(stats.minTrisPerLeaf, node.numTris);
			depthSum += depth;
		}
		else // Interior node
		{
			stats.interiorNodes++;
			traverse(node.leftChild, depth + 1);
			traverse(node.leftChild + 1, depth + 1);
		}
	};

	traverse(0, 0);

	if (stats.leafNodes > 0)
	{
		stats.avgTrisPerLeaf = static_cast<float>(stats.totalTriangles) / stats.leafNodes;
		stats.avgDepth = static_cast<float>(depthSum) / stats.leafNodes;
	}
	if (stats.minTrisPerLeaf == UINT32_MAX)
		stats.minTrisPerLeaf = 0;

	return stats;
}

void BVHBuilder::PrintStats(const BVHStats& stats)
{
	std::cout << "=== BVH Statistics ===" << std::endl;
	std::cout << "Total nodes:      " << stats.totalNodes << std::endl;
	std::cout << "  Interior:       " << stats.interiorNodes << std::endl;
	std::cout << "  Leaves:         " << stats.leafNodes << std::endl;
	std::cout << "Total triangles:  " << stats.totalTriangles << std::endl;
	std::cout << "Max depth:        " << stats.maxDepth << std::endl;
	std::cout << "Avg leaf depth:   " << stats.avgDepth << std::endl;
	std::cout << "Tris per leaf:    min=" << stats.minTrisPerLeaf
	          << " max=" << stats.maxTrisPerLeaf
	          << " avg=" << stats.avgTrisPerLeaf << std::endl;

	float idealDepth = std::log2(static_cast<float>(stats.totalTriangles));
	std::cout << "Ideal depth:      ~" << idealDepth << std::endl;

	if (stats.maxDepth > idealDepth * 2)
		std::cout << "WARNING: BVH is unbalanced! Consider using SAH." << std::endl;
	if (stats.avgTrisPerLeaf > 16)
		std::cout << "WARNING: Too many tris per leaf. Consider smaller leaf threshold." << std::endl;
	if (stats.maxTrisPerLeaf > 64)
		std::cout << "WARNING: Some leaves have many triangles (" << stats.maxTrisPerLeaf << ")." << std::endl;

	std::cout << "======================" << std::endl;
}

std::vector<Node> BVHBuilder::BuildBVH()
{
	Node rootNode{};
	rootNode.firstTriIndex = 0;
	rootNode.numTris = static_cast<uint32_t>(tris.size());

	if (rootNode.numTris == 0)
		return nodes;

	nodes.push_back(rootNode);
	nodesUsed = 0;
	UpdateBounds(nodesUsed);
	SplitNode(nodesUsed, 0);
	return nodes;
}

