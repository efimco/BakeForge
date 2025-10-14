#include "sceneNode.hpp"
#include <iostream>
#include "sceneManager.hpp"

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
	if (child->parent)
	{
		child->parent->removeChild(child);
	}
	child->parent = this;
	children.push_back(child);
}

void SceneNode::removeChild(SceneNode* child)
{
	if (child->parent == this)
	{
		child->parent = nullptr;
		auto it = std::find(children.begin(), children.end(), child);
		if (it != children.end())
		{
			children.erase(it);
		}
		else
		{
			std::cerr << "Error: Child not found in parent's children list." << std::endl;
		}
	}
}

