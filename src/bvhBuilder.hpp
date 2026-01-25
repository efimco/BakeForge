#pragma once

#include "bvhNode.hpp"


namespace BVH
{
	struct BVHStats
	{
		uint32_t totalNodes = 0;
		uint32_t leafNodes = 0;
		uint32_t interiorNodes = 0;
		uint32_t maxDepth = 0;
		uint32_t totalTriangles = 0;
		uint32_t maxTrisPerLeaf = 0;
		uint32_t minTrisPerLeaf = UINT32_MAX;
		float avgTrisPerLeaf = 0.0f;
		float avgDepth = 0.0f;
	};

	class BVHBuilder
	{
	public:
		explicit BVHBuilder(const std::vector<Triangle>& triangles, std::vector<uint32_t>& trisIndex_);
		std::vector<Node> BuildBVH();

		static BVHStats CalculateStats(const std::vector<Node>& nodes);
		static void PrintStats(const BVHStats& stats);

	private:
		std::vector<Node> nodes = std::vector<Node>();
		uint32_t nodesUsed = 0;
		const std::vector<Triangle>& tris;
		std::vector<uint32_t>& trisIndex;

		void SplitNode(uint32_t nodeIndex, uint32_t depth);
		void UpdateBounds(uint32_t index);
	};
} // namespace BVH
