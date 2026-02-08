#pragma once

#include "sceneNodeHandle.hpp"
#include "commandBase.hpp"

class SceneNode;
class Scene;

namespace Command
{

	class DuplicateSceneNode final : public CommandBase
	{
	public:
		DuplicateSceneNode(Scene* inScene, SceneNode* inSceneNode, int index = -1, bool isRestoreOp = false);

		void addChild(SceneNode* childNode);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		SceneNodeHandle m_parentHandle;
		std::vector<std::pair<SceneNodeHandle, int>> m_childHandles;
		std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
		int m_index = -1;
		bool m_isRestoreOp = false;
	};

	class RemoveSceneNode final : public CommandBase
	{
	public:
		RemoveSceneNode(Scene* inScene, SceneNode* inSceneNode);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
	};

	class ReparentSceneNode final : public CommandBase
	{
	public:
		ReparentSceneNode(Scene* inScene, SceneNode* inSceneNode, SceneNode* inNewParent, int index = -1);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		SceneNodeHandle m_newParentHandle;
		int m_index = -1;
	};

};
