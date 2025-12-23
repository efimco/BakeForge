
#include "nodeSnapshot.hpp"

#include "sceneNode.hpp"

namespace Snapshot
{

SceneNodeCopy::SceneNodeCopy(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	assert(inSceneNode);
	m_sceneNodeClone = m_sceneNode->clone();
}

std::unique_ptr<CommandBase> SceneNodeCopy::exec()
{
    auto redoTransaction = std::make_unique<SceneNodeCopy>(m_scene, m_sceneNode);
    m_sceneNode->copyFrom(m_sceneNodeClone.get());
    return redoTransaction;
}

bool SceneNodeCopy::merge(SnapshotBase* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (auto* castCommand = dynamic_cast<SceneNodeCopy*>(command))
		{
			if (castCommand->m_sceneNode == m_sceneNode)
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
	return m_sceneNodeClone->differsFrom(m_sceneNode);
}

void SceneNodeCopy::onCommitTransaction()
{
	m_sceneNode->onCommitTransaction(m_scene);
}

SceneNodeTransform::SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	m_savedTransform = m_sceneNode->transform;
}

std::unique_ptr<CommandBase> SceneNodeTransform::exec()
{
    auto redoTransaction = std::make_unique<SceneNodeTransform>(m_scene, m_sceneNode);
    m_sceneNode->transform = m_savedTransform;
    return redoTransaction;
}

bool SceneNodeTransform::merge(SnapshotBase* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (const auto* castCommand = dynamic_cast<SceneNodeTransform*>(command))
		{
			if (castCommand->m_sceneNode == m_sceneNode)
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
	return
		m_sceneNode->transform.position != m_savedTransform.position ||
		m_sceneNode->transform.rotation != m_savedTransform.rotation ||
		m_sceneNode->transform.scale != m_savedTransform.scale;
}

void SceneNodeTransform::onCommitTransaction()
{
	m_sceneNode->onCommitTransaction(m_scene);
}

}
