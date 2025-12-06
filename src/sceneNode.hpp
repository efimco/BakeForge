#pragma once
#include "transform.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class SceneNode
{
public:
	Transform transform;
	std::vector<std::unique_ptr<SceneNode>> children;
	SceneNode* parent = nullptr;
	bool visible = true;
	bool dirty = false;
	bool movable = true;
	std::string name;

	SceneNode(std::string name = "SceneNode");
	SceneNode(const SceneNode&) = delete;
	SceneNode& operator=(const SceneNode&) = delete;
	SceneNode(SceneNode&& other) noexcept;
	virtual ~SceneNode();

	void addChild(std::unique_ptr<SceneNode>&& child);
	std::unique_ptr<SceneNode> removeChild(SceneNode* child);
	glm::mat4 getWorldMatrix();
};