#pragma once

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
			BakerPass* bakerPass);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;

		std::shared_ptr<TextureHistory> m_textureHistory;
		std::shared_ptr<TextureDelta> m_textureDelta;
		BakerPass* m_bakerPass = nullptr;
	};

	// This command would create a delta for a blend mask in normal map baker,
	// undo operation on this command would restore texture snapshot.
	class BakerBlendMaskCreateDeltaCommand final : public CommandBase
	{
	public:
		BakerBlendMaskCreateDeltaCommand(
			std::shared_ptr<TextureHistory> textureHistory,
			BakerPass* bakerPass);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;

		std::shared_ptr<TextureHistory> m_textureHistory;
		BakerPass* m_bakerPass = nullptr;
	};

}
