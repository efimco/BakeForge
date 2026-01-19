#pragma once

#include "primitiveData.hpp"

using Position = glm::vec3;

namespace BVH
{

	struct BBox
	{
		Position min;
		Position max;
	};

	struct Node
	{
		BBox bbox;
		uint32_t leftChild, firstTriIndex, numTris;
		bool isLeaf()
		{
			return numTris > 0;
		}
	};
}; // namespace BVH
