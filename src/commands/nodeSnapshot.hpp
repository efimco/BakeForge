#pragma once

#include "transform.hpp"
#include "sceneNodeHandle.hpp"
#include "snapshotBase.hpp"

class SceneNode;
class Scene;

namespace Snapshot
{

	// This command would save a snapshot of the scene node.
	// This allows you to encompass arbitrary property changes.
	// Be careful, as it might be excessively expensive to do this for some node types.
	class SceneNodeCopy final : public SnapshotBase
	{
	public:
		SceneNodeCopy(Scene* inScene, SceneNode* inSceneNode);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;
		virtual bool merge(SnapshotBase* command) override;
		virtual bool containsChanges() const override;
		virtual void onCommitTransaction() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
	};

	// This command would only save a snapshot of the scene node transform.
	class SceneNodeTransform final : public SnapshotBase
	{
	public:
		SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;
		virtual bool merge(SnapshotBase* command) override;
		virtual bool containsChanges() const override;
		virtual void onCommitTransaction() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		Transform m_savedTransform;
	};

}
