#pragma once

#include "sceneNodeHandle.hpp"
#include "snapshotBase.hpp"
#include "textureHistory.hpp"

class BakerPass;
class SceneNode;
class Scene;

namespace Command
{

	static constexpr std::string_view k_blendPaintName { "BlendPaint" };

	// This command would apply provided TextureDelta on exec()
	class BakerBlendMaskApplyDeltaCommand final : public CommandBase
	{
	public:
		BakerBlendMaskApplyDeltaCommand(
			std::shared_ptr<TextureHistory> textureHistory,
			std::shared_ptr<TextureDelta> textureDelta,
			std::shared_ptr<BakerPass> bakerPass);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;

		std::shared_ptr<TextureHistory> m_textureHistory;
		std::shared_ptr<TextureDelta> m_textureDelta;
		std::shared_ptr<BakerPass> m_bakerPass = nullptr;
	};

	// This command would create a delta for a blend mask in normal map baker,
	// undo operation on this command would restore texture snapshot.
	class BakerBlendMaskCreateDeltaCommand final : public CommandBase
	{
	public:
		BakerBlendMaskCreateDeltaCommand(
			std::shared_ptr<TextureHistory> textureHistory,
			std::shared_ptr<BakerPass> bakerPass);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;

		std::shared_ptr<TextureHistory> m_textureHistory;
		std::shared_ptr<BakerPass> m_bakerPass = nullptr;
	};

	class ToggleSmoothNormalsCommand final : public CommandBase
	{
	public:
		ToggleSmoothNormalsCommand(Scene* inScene, SceneNode* inSceneNode, bool newValue);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		bool m_newValue = false;
	};

}
