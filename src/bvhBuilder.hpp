#pragma once

#include "bvhNode.hpp"


namespace BVH
{
	class BVHBuilder
	{
	public:
		explicit BVHBuilder(const std::vector<Triangle>& triangles, std::vector<uint32_t>& trisIndex_);
		std::vector<Node> BuildBVH();

	private:
		std::vector<Node> nodes = std::vector<Node>();
		uint32_t nodesUsed = 0;
		const std::vector<Triangle>& tris;
		std::vector<uint32_t>& trisIndex;

		void SplitNode(uint32_t nodeIndex);
		void UpdateBounds(uint32_t index);
	};
} // namespace BVH
