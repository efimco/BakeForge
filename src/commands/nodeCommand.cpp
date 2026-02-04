#include "nodeCommand.hpp"

#include <cassert>

#include "scene.hpp"

namespace Command
{

	DuplicateSceneNode::DuplicateSceneNode(Scene* inScene, SceneNode* inSceneNode, const bool reuseNodeHandle)
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

	RestoreSceneNode::RestoreSceneNode(Scene* inScene, SceneNode* inSceneNode)
		: m_scene(inScene)
		, m_nodeHandle(inScene->findHandleOfNode(inSceneNode))
	{
		assert(inSceneNode);
		m_sceneNodeClone = inSceneNode->clone();
	}

	void RestoreSceneNode::addChild(SceneNode* childNode)
	{
		m_childHandles.push_back(m_scene->findHandleOfNode(childNode));
	}

	std::unique_ptr<CommandBase> RestoreSceneNode::exec()
	{
		SceneNode* clonedNode = m_scene->adoptClonedNode(std::move(m_sceneNodeClone), m_nodeHandle);
		for (SceneNodeHandle childHandle : m_childHandles)
		{
			SceneNode* child = m_scene->getNodeByHandle(childHandle);
			int childIndex = child->getChildIndex(child);
			std::unique_ptr<SceneNode> childNode = child->parent->removeChild(child);
			clonedNode->addChild(std::move(childNode), childIndex);
		}
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
		auto restoreCommand = std::make_unique<RestoreSceneNode>(m_scene, sceneNode);
		if (sceneNode->parent)
		{
			while (!sceneNode->children.empty())
			{
				std::unique_ptr<SceneNode> child = std::move(sceneNode->children.back());
				restoreCommand->addChild(child.get());
				sceneNode->parent->addChild(std::move(child));
				sceneNode->children.pop_back();
			}
		}
		m_scene->deleteNode(sceneNode);
		return restoreCommand;
	}

	ReparentSceneNode::ReparentSceneNode(Scene* inScene, SceneNode* inSceneNode, SceneNode* inNewParent, int index)
	{
		m_scene = inScene;
		m_nodeHandle = inScene->findHandleOfNode(inSceneNode);
		m_newParentIsScene = inNewParent == inScene;
		m_oldParentIsScene = inSceneNode->parent == inScene;
		m_newParentHandle = inScene->findHandleOfNode(inNewParent);
		m_oldParentHandle = inScene->findHandleOfNode(inSceneNode->parent);
		m_index = index;
	}

	std::unique_ptr<CommandBase> ReparentSceneNode::exec()
	{
		SceneNode* node = m_scene->getNodeByHandle(m_nodeHandle);
		SceneNode* newParentNode = m_newParentIsScene ? m_scene : m_scene->getNodeByHandle(m_newParentHandle);
		SceneNode* oldParentNode = m_oldParentIsScene ? m_scene : m_scene->getNodeByHandle(m_oldParentHandle);

		int oldIndex = oldParentNode->getChildIndex(node);

		std::unique_ptr<SceneNode> nodePtr = oldParentNode->removeChild(node);
		newParentNode->addChild(std::move(nodePtr), m_index);

		auto reparentCommand = std::make_unique<ReparentSceneNode>(m_scene, node, oldParentNode, oldIndex);
		return reparentCommand;
	}

}
