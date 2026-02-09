#include "normalBakerCommand.hpp"

#include "bakerPass.hpp"

namespace Command
{
	BakerBlendMaskApplyDeltaCommand::BakerBlendMaskApplyDeltaCommand(
		std::shared_ptr<TextureHistory> textureHistory,
		std::shared_ptr<TextureDelta> textureDelta,
		BakerPass* bakerPass)
		: m_textureHistory(textureHistory)
		, m_textureDelta(textureDelta)
		, m_bakerPass(bakerPass)
	{
	}

	std::unique_ptr<CommandBase> BakerBlendMaskApplyDeltaCommand::exec()
	{
		m_textureHistory->StartSnapshot(k_blendPaintName, m_bakerPass->getBlendTexture(), true);
		m_textureHistory->ApplyDelta(m_bakerPass->getBlendTexture(), m_textureDelta);
		m_bakerPass->needsRebake = true;
		auto textureDelta =m_textureHistory->CreateDelta(
			k_blendPaintName,
			m_bakerPass->getBlendTexture(),
			m_bakerPass->getBlendTextureSRV());
		m_textureHistory->EndSnapshot(k_blendPaintName);
		return std::make_unique<BakerBlendMaskApplyDeltaCommand>(m_textureHistory, textureDelta, m_bakerPass);
	}

	BakerBlendMaskCreateDeltaCommand::BakerBlendMaskCreateDeltaCommand(
		std::shared_ptr<TextureHistory> textureHistory,
		BakerPass* bakerPass)
		: m_textureHistory(textureHistory)
		, m_bakerPass(bakerPass)
	{
	}

	std::unique_ptr<CommandBase> BakerBlendMaskCreateDeltaCommand::exec()
	{
		auto textureDelta = m_textureHistory->CreateDelta(
			k_blendPaintName,
			m_bakerPass->getBlendTexture(),
			m_bakerPass->getBlendTextureSRV());
		return std::make_unique<BakerBlendMaskApplyDeltaCommand>(m_textureHistory, textureDelta, m_bakerPass);
	}
}
