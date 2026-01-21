#pragma once

#include "primitiveData.hpp"

using Position = glm::vec3;

namespace BVH
{

	struct BBox
	{
		Position min;
		float pad0;
		Position max;
		float pad1;
	};

	struct Node
	{
		BBox bbox;
		uint32_t leftChild, firstTriIndex, numTris;
		uint32_t depth;
		bool isLeaf()
		{
			return numTris > 0;
		}
	};
}; // namespace BVH
