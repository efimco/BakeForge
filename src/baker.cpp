#include "baker.hpp"

LowPolyNode::LowPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
}

HighPolyNode::HighPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
}

Baker::Baker(const std::string_view nodeName)
{
	name = nodeName;
	movable = false;
	lowPoly = std::make_unique<LowPolyNode>("LowPolyContainer");
	highPoly = std::make_unique<HighPolyNode>("HighPolyContainer");
}

Baker::~Baker()
{
}
