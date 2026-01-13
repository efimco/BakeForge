#pragma once
#include "sceneNode.hpp"


class BakerNode : public SceneNode
{
public:
	explicit BakerNode() = default;
	~BakerNode() override = default;
};

class LowPolyNode : public BakerNode
{
public:
	explicit LowPolyNode(const std::string_view nodeName);
	~LowPolyNode() override = default;
};

class HighPolyNode : public BakerNode
{
public:
	explicit HighPolyNode(const std::string_view nodeName);
	~HighPolyNode() override = default;
};

class Baker : public SceneNode
{
public:
	explicit Baker(std::string_view nodeName);
	~Baker() override;

	std::unique_ptr<LowPolyNode> lowPoly;
	std::unique_ptr<HighPolyNode> highPoly;
};
