#include "nodeCommand.hpp"

#include "scene.hpp"

#include <cassert>

namespace Command
{

DuplicateSceneNode::DuplicateSceneNode(
	Scene* inScene,
	SceneNode* inSceneNode,
	bool reuseNodeHandle)
	: m_scene(inScene)
	, m_nodeHandle(reuseNodeHandle ? inScene->findHandleOfNode(inSceneNode) : SceneNodeHandle::invalidHandle())
	, m_validateName(!reuseNodeHandle)
{
	assert(inSceneNode);
	m_sceneNodeClone = inSceneNode->clone();
}

std::unique_ptr<CommandBase> DuplicateSceneNode::exec()
{
	if (m_validateName)
	{
		m_scene->validateName(m_sceneNodeClone.get());
	}
	SceneNode* clonedNode = m_scene->adoptClonedNode(std::move(m_sceneNodeClone), m_nodeHandle);
	return std::make_unique<RemoveSceneNode>(m_scene, clonedNode);
}

RemoveSceneNode::RemoveSceneNode(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_nodeHandle(inScene->findHandleOfNode(inSceneNode))
{
	assert(inSceneNode);
}

std::unique_ptr<CommandBase> RemoveSceneNode::exec()
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	auto createCommand = std::make_unique<DuplicateSceneNode>(m_scene, sceneNode, true);
	m_scene->deleteNode(sceneNode);
	return createCommand;
}

}
