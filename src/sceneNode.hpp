#pragma once

#include <memory>
#include <string>
#include <list>

#include <glm/glm.hpp>

#include "transform.hpp"

class Scene;

class SceneNode
{
public:
	Transform transform;
	std::string name;
	std::list<std::unique_ptr<SceneNode>> children;
	SceneNode* parent = nullptr;

	explicit SceneNode(std::string_view nodeName = "SceneNode");
	SceneNode(const SceneNode&) = delete;
	SceneNode(SceneNode&& other) noexcept;
	SceneNode& operator=(const SceneNode&) = delete;
	SceneNode& operator=(SceneNode&&) = delete;
	virtual ~SceneNode();

	virtual void onCommitTransaction(Scene& scene);
	virtual void copyFrom(const SceneNode& node);
	virtual bool differsFrom(const SceneNode& node) const;
	virtual std::unique_ptr<SceneNode> clone() const;
	void addChild(std::unique_ptr<SceneNode>&& child);
	std::unique_ptr<SceneNode> removeChild(SceneNode* child);
	glm::mat4 getWorldMatrix();
};
