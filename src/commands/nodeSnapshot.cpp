
#include "nodeSnapshot.hpp"

#include "scene.hpp"
#include "sceneNode.hpp"

namespace Snapshot
{

SceneNodeCopy::SceneNodeCopy(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_nodeHandle(inScene->getHandleOfNode(inSceneNode))
{
	assert(inSceneNode);
	m_sceneNodeClone = inSceneNode->clone();
}

std::unique_ptr<CommandBase> SceneNodeCopy::exec()
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	auto redoTransaction = std::make_unique<SceneNodeCopy>(m_scene, sceneNode);
	sceneNode->copyFrom(m_sceneNodeClone.get());
	return redoTransaction;
}

bool SceneNodeCopy::merge(SnapshotBase* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (auto* castCommand = dynamic_cast<SceneNodeCopy*>(command))
		{
			if (castCommand->m_nodeHandle == m_nodeHandle)
			{
				m_sceneNodeClone = std::move(castCommand->m_sceneNodeClone);
				castCommand->m_sceneNodeClone = nullptr;
				return true;
			}
		}
	}
	return false;
}

bool SceneNodeCopy::containsChanges() const
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	return m_sceneNodeClone->differsFrom(sceneNode);
}

void SceneNodeCopy::onCommitTransaction()
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	sceneNode->onCommitTransaction(m_scene);
}

SceneNodeTransform::SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_nodeHandle(inScene->getHandleOfNode(inSceneNode))
{
	m_savedTransform = inSceneNode->transform;
}

std::unique_ptr<CommandBase> SceneNodeTransform::exec()
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	auto redoTransaction = std::make_unique<SceneNodeTransform>(m_scene, sceneNode);
	sceneNode->transform = m_savedTransform;
	return redoTransaction;
}

bool SceneNodeTransform::merge(SnapshotBase* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (const auto* castCommand = dynamic_cast<SceneNodeTransform*>(command))
		{
			if (castCommand->m_nodeHandle == m_nodeHandle)
			{
				m_savedTransform = castCommand->m_savedTransform;
				return true;
			}
		}
	}
	return false;
}

bool SceneNodeTransform::containsChanges() const
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	return
		sceneNode->transform.position != m_savedTransform.position ||
		sceneNode->transform.rotation != m_savedTransform.rotation ||
		sceneNode->transform.scale != m_savedTransform.scale;
}

void SceneNodeTransform::onCommitTransaction()
{
	SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
	assert(sceneNode);
	sceneNode->onCommitTransaction(m_scene);
}

}
