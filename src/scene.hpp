#pragma once

#include "sceneNode.hpp"


class Scene : public SceneNode
{
public:
	Scene(std::string name = "Scene Root") { this->name = name; }
	~Scene() = default;

	SceneNode* getRootNode() { return children.front(); }

private:
	SceneNode m_rootNode;
};