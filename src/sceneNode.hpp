#pragma once

#include <list>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "sceneNodeHandle.hpp"
#include "transform.hpp"

class Scene;

class SceneNode
{
public:
	Transform transform;
	std::string name;
	std::list<std::unique_ptr<SceneNode>> children;
	SceneNode* parent = nullptr;
	bool movable = true;
	bool deletable = true;
	bool canBecomeParent = true;

	explicit SceneNode(std::string_view nodeName = "SceneNode");
	SceneNode(const SceneNode&) = delete;
	SceneNode(SceneNode&& other) noexcept;
	SceneNode& operator=(const SceneNode&) = delete;
	SceneNode& operator=(SceneNode&&) = delete;
	virtual ~SceneNode();

	virtual bool getVisibility() const;
	virtual void setVisibility(bool isVisible);

	virtual void onCommitTransaction(Scene& scene);
	virtual void copyFrom(const SceneNode& node);
	virtual bool differsFrom(const SceneNode& node) const;
	virtual std::unique_ptr<SceneNode> clone() const;

	int getChildIndex(const SceneNode* child) const;
	glm::mat4 getWorldMatrix();
	SceneNodeHandle getNodeHandle() const { return nodeHandle; }

protected:
	virtual void addChild(std::unique_ptr<SceneNode>&& child, int index = -1);
	std::unique_ptr<SceneNode> removeChild(SceneNode* child);
	virtual void clearChildren();

	bool visible = true;

	friend class Scene;
	SceneNodeHandle nodeHandle = SceneNodeHandle::invalidHandle();
};
