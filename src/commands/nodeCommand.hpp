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
		DuplicateSceneNode(Scene* inScene, SceneNode* inSceneNode, const bool reuseNodeHandle = false);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
		bool m_validateName = true;
	};

	class RestoreSceneNode final : public CommandBase
	{
	public:
		RestoreSceneNode(Scene* inScene, SceneNode* inSceneNode);

		void addChild(SceneNode* childNode);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		std::vector<SceneNodeHandle> m_childHandles;
		std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
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
		SceneNodeHandle m_oldParentHandle;
		bool m_oldParentIsScene = false;
		bool m_newParentIsScene = false;
		int m_index = -1;
	};

};
