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

void SceneNode::addChild(std::unique_ptr<SceneNode>&& child)
{
	if (child->parent)
	{
		child->parent->removeChild(child.get());
	}
	child->parent = this;
	children.push_back(std::move(child));
}

void SceneNode::removeChild(SceneNode* child)
{
	if (child->parent == this)
	{
		child->parent = nullptr;
		auto isDesiredChild = [child](const std::unique_ptr<SceneNode>& ch) {return ch.get() == child;};
		auto it = std::remove_if(children.begin(), children.end(), isDesiredChild);
		if (it != children.end())
		{
			children.erase(it, children.end());
		}
		else
		{
			std::cerr << "Error: Child not found in parent's children list." << std::endl;
		}
	}
}

