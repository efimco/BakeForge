#include "normalBakerCommand.hpp"

#include "bakerNode.hpp"
#include "bakerPass.hpp"
#include "scene.hpp"
#include "sceneNode.hpp"

namespace Command
{
	BakerBlendMaskApplyDeltaCommand::BakerBlendMaskApplyDeltaCommand(
		std::shared_ptr<TextureHistory> textureHistory,
		std::shared_ptr<TextureDelta> textureDelta,
		std::shared_ptr<BakerPass> bakerPass)
		: m_textureHistory(textureHistory)
		, m_textureDelta(textureDelta)
		, m_bakerPass(bakerPass)
	{
	}

	std::unique_ptr<CommandBase> BakerBlendMaskApplyDeltaCommand::exec()
	{
		// Create a new delta map from the previous list of indices
		auto textureDelta = m_textureHistory->createDelta(
			m_bakerPass->getBlendTexture(),
			m_textureDelta->m_tileIndices);
		m_textureHistory->applyDelta(
			m_bakerPass->getBlendTexture(),
			m_textureDelta);
		m_bakerPass->needsRebake = true;
		return std::make_unique<BakerBlendMaskApplyDeltaCommand>(m_textureHistory, textureDelta, m_bakerPass);
	}

	BakerBlendMaskCreateDeltaCommand::BakerBlendMaskCreateDeltaCommand(
		std::shared_ptr<TextureHistory> textureHistory,
		std::shared_ptr<BakerPass> bakerPass)
		: m_textureHistory(textureHistory)
		, m_bakerPass(bakerPass)
	{
	}

	std::unique_ptr<CommandBase> BakerBlendMaskCreateDeltaCommand::exec()
	{
		auto textureDelta = m_textureHistory->createDelta(
			k_blendPaintName,
			m_bakerPass->getBlendTexture(),
			m_bakerPass->getBlendTextureSRV());
		return std::make_unique<BakerBlendMaskApplyDeltaCommand>(m_textureHistory, textureDelta, m_bakerPass);
	}

	ToggleSmoothNormalsCommand::ToggleSmoothNormalsCommand(
		Scene* inScene,
		SceneNode* inSceneNode,
		bool newValue)
		: m_scene(inScene)
		, m_nodeHandle(inSceneNode->getNodeHandle())
		, m_newValue(newValue)
	{
	}

	std::unique_ptr<CommandBase> ToggleSmoothNormalsCommand::exec()
	{
		SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
		assert(sceneNode);

		Baker* bakerNode = dynamic_cast<Baker*>(sceneNode);
		assert(bakerNode);

		bakerNode->useSmoothedNormals = m_newValue;
		bakerNode->requestBake();
		return std::make_unique<ToggleSmoothNormalsCommand>(m_scene, sceneNode, !m_newValue);
	}
}
