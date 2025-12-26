#include "sceneNode.hpp"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include "scene.hpp"

std::atomic_int32_t SceneNodeHandle::s_handleGenerator = 0;

SceneNode::SceneNode(SceneNode&& other) noexcept
	: transform(other.transform)
	, children(std::move(other.children))
	, visible(other.visible)
	, dirty(other.dirty)
	, movable(other.movable)
{
	this->parent = std::move(other.parent);
	other.parent = nullptr;
	other.visible = false;
	other.dirty = false;
	other.movable = false;
	other.transform = Transform();
	other.children.clear();
}

SceneNode::SceneNode(std::string name)
	: parent(nullptr)
{
	this->name = name;
}

SceneNode::~SceneNode() = default;

void SceneNode::onCommitTransaction(Scene& scene)
{
	scene.markSceneBVHDirty();
}

void SceneNode::copyFrom(const SceneNode& node)
{
	transform = node.transform;
	visible = node.visible;
	movable = node.movable;
	name = node.name;
}

bool SceneNode::differsFrom(const SceneNode& node) const
{
	return
		transform.position != node.transform.position ||
		transform.rotation != node.transform.rotation ||
		transform.scale != node.transform.scale ||
		visible != node.visible ||
		movable != node.movable;
}

std::unique_ptr<SceneNode> SceneNode::clone() const
{
	std::unique_ptr<SceneNode> newNode = std::make_unique<SceneNode>(this->name);
	newNode->copyFrom(*this);
	return newNode;
}

void SceneNode::addChild(std::unique_ptr<SceneNode>&& child)
{
	if (!child)
		return;

	glm::mat4 worldTransform = child->getWorldMatrix();

	if (child->parent)
	{
		child->parent->removeChild(child.get());
	}

	child->parent = this;
	SceneNode* childPtr = child.get(); // Get a raw pointer before moving
	children.push_back(std::move(child));

	if (dynamic_cast<Scene*>(this))
	{
		// For Scene root, the world matrix IS the local matrix
		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 position;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(worldTransform, scale, rotation, position, skew, perspective);

		childPtr->transform.position = position;
		childPtr->transform.rotation = glm::degrees(glm::eulerAngles(rotation));
		childPtr->transform.scale = scale;
		childPtr->transform.updateMatrix();
		return;
	}
	glm::mat4 parentWorldInverse = glm::inverse(this->getWorldMatrix());
	glm::mat4 newLocalMatrix = parentWorldInverse * worldTransform;

	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 position;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(newLocalMatrix, scale, rotation, position, skew, perspective);

	childPtr->transform.position = position;
	childPtr->transform.rotation = glm::degrees(glm::eulerAngles(rotation));
	childPtr->transform.scale = scale;
	childPtr->transform.updateMatrix();
}

std::unique_ptr<SceneNode> SceneNode::removeChild(SceneNode* child)
{
	if (child->parent != this)
		return nullptr;

	glm::mat4 childWorldMatrix = child->getWorldMatrix();

	child->parent = nullptr;
	auto it = std::ranges::find_if(children,
	   [child](const std::unique_ptr<SceneNode>& ptr)
	   {
		   return ptr.get() == child;
	   });
	if (it == children.end())
	{
		std::cerr << "Error: Child not found in parent's children list." << std::endl;
		return nullptr;
	}

	auto removedChild = std::move(*it);
	children.erase(it);

	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 position;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(childWorldMatrix, scale, rotation, position, skew, perspective);
	child->transform.position = position;
	child->transform.rotation = glm::degrees(glm::eulerAngles(rotation));
	child->transform.scale = scale;
	child->transform.updateMatrix();
	return removedChild;
}

glm::mat4 SceneNode::getWorldMatrix()
{
	transform.updateMatrix();
	if (parent)
	{
		return parent->getWorldMatrix() * transform.matrix;
	}
	return transform.matrix;
};
