#include "sceneNode.hpp"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include "scene.hpp"

SceneNode::SceneNode(SceneNode&& other) noexcept
	: transform(other.transform), children(std::move(other.children)), visible(other.visible), dirty(other.dirty),
	movable(other.movable)
{
	this->parent = std::move(other.parent);
	other.parent = nullptr;
	other.visible = false;
	other.dirty = false;
	other.movable = false;
	other.transform = Transform();
	other.children.clear();
}

SceneNode::SceneNode() : parent(nullptr) {};

SceneNode::~SceneNode() = default;

void SceneNode::addChild(SceneNode* child)
{
	glm::mat4 worldTransform = child->getWorldMatrix();

	if (child->parent)
	{
		child->parent->removeChild(child);
	}

	child->parent = this;
	children.push_back(child);

	if (dynamic_cast<Scene*>(this))
	{
		// For Scene root, the world matrix IS the local matrix
		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 position;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(worldTransform, scale, rotation, position, skew, perspective);

		child->transform.position = position;
		child->transform.rotation = glm::degrees(glm::eulerAngles(rotation));
		child->transform.scale = scale;
		child->transform.updateMatrix();
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

	child->transform.position = position;
	child->transform.rotation = glm::degrees(glm::eulerAngles(rotation));
	child->transform.scale = scale;
	child->transform.updateMatrix();

}

void SceneNode::removeChild(SceneNode* child)
{
	if (child->parent != this)
		return;


	glm::mat4 childWorldMatrix = child->getWorldMatrix();


	child->parent = nullptr;
	auto it = std::find(children.begin(), children.end(), child);

	if (it == children.end())
	{
		std::cerr << "Error: Child not found in parent's children list." << std::endl;
		return;
	}

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

