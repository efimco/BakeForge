#include "nodeCommand.hpp"

#include <cassert>

#include "scene.hpp"

namespace Command
{

	DuplicateSceneNode::DuplicateSceneNode(Scene* inScene, SceneNode* inSceneNode, int index, bool isRestoreOp)
		: m_scene(inScene)
		, m_nodeHandle(inSceneNode->getNodeHandle())
		, m_index(index)
		, m_isRestoreOp(isRestoreOp)
	{
		assert(inSceneNode);
		assert(inSceneNode->parent);
		assert(m_nodeHandle.isValid());
		m_parentHandle = inSceneNode->parent->getNodeHandle();
		m_sceneNodeClone = inSceneNode->clone();
	}

	void DuplicateSceneNode::addChild(SceneNode* childNode)
	{
		assert(childNode);
		assert(childNode->parent);
		int childIndex = childNode->parent->getChildIndex(childNode);
		m_childHandles.emplace_back(childNode->getNodeHandle(), childIndex);
	}

	std::unique_ptr<CommandBase> DuplicateSceneNode::exec()
	{
		// If we're restoring previously deleted node - reuse old name and handle (skip validating name)
		SceneNodeHandle nodeHandle = m_isRestoreOp ? m_nodeHandle : SceneNodeHandle::invalidHandle();
		bool shouldValidateName = !m_isRestoreOp;
		SceneNode* clonedNode = m_scene->adoptClonedNode(std::move(m_sceneNodeClone), nodeHandle, shouldValidateName);

		m_scene->reparentChild(clonedNode, m_scene->getNodeByHandle(m_parentHandle), m_index);

		// First command in group should reset selection, consecutive should add to selection
		bool shouldAddToSelection = getNumberInGroup() != k_invalidNumberInGroup && getNumberInGroup() != 0;
		m_scene->setActiveNode(clonedNode, shouldAddToSelection);

		for (auto& [childHandle, childIndex] : m_childHandles)
		{
			m_scene->reparentChild(m_scene->getNodeByHandle(childHandle), clonedNode, childIndex);
			m_scene->setActiveNode(clonedNode, true);
		}
		return std::make_unique<RemoveSceneNode>(m_scene, clonedNode);
	}

	RemoveSceneNode::RemoveSceneNode(Scene* inScene, SceneNode* inSceneNode)
		: m_scene(inScene)
		, m_nodeHandle(inSceneNode->getNodeHandle())
	{
		assert(inSceneNode);
		assert(inSceneNode->parent);
		assert(m_nodeHandle.isValid());
	}

	std::unique_ptr<CommandBase> RemoveSceneNode::exec()
	{
		SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
		assert(sceneNode);
		assert(sceneNode->parent);

		// Reparent child nodes such that they start at the same index as the node that is being removed
		int nodeIndex = sceneNode->parent->getChildIndex(sceneNode);
		auto restoreCommand = std::make_unique<DuplicateSceneNode>(m_scene, sceneNode, nodeIndex, true);

		// You're calling the command on a node that has been cloned but was not attached to the scene.
		assert(sceneNode->parent);
		while (!sceneNode->children.empty())
		{
			SceneNode* childNode = sceneNode->children.back().get();
			restoreCommand->addChild(childNode);
			m_scene->reparentChild(childNode, sceneNode->parent, nodeIndex);
		}
		m_scene->deleteNode(sceneNode);
		return restoreCommand;
	}

	ReparentSceneNode::ReparentSceneNode(Scene* inScene, SceneNode* inSceneNode, SceneNode* inNewParent, int index)
		: m_scene(inScene)
		, m_nodeHandle(inSceneNode->getNodeHandle())
		, m_newParentHandle(inNewParent->getNodeHandle())
		, m_index(index)
	{
		assert(inSceneNode);
		assert(inSceneNode->parent);
		assert(inNewParent);
		assert(m_nodeHandle.isValid());
		assert(m_newParentHandle.isValid());
	}

	std::unique_ptr<CommandBase> ReparentSceneNode::exec()
	{
		SceneNode* node = m_scene->getNodeByHandle(m_nodeHandle);
		SceneNode* newParentNode = m_scene->getNodeByHandle(m_newParentHandle);
		SceneNode* oldParentNode = node->parent;
		int nodeIndex = oldParentNode->getChildIndex(node);

		m_scene->reparentChild(node, newParentNode, m_index);

		auto reparentCommand = std::make_unique<ReparentSceneNode>(m_scene, node, oldParentNode, nodeIndex);
		return reparentCommand;
	}

}
