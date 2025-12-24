#pragma once

#include "snapshotBase.hpp"
#include "transform.hpp"

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

		// TO-DO: switch from pointers to handles -
		// this would prevent them from being invalidated,
		// which would allow you to keep
		Scene* m_scene = nullptr;
		SceneNode* m_sceneNode = nullptr;
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

		// TO-DO: switch from pointers to handles
		Scene* m_scene = nullptr;
		SceneNode* m_sceneNode = nullptr;
		Transform m_savedTransform;
	};

};
