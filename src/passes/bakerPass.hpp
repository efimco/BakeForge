#pragma once

#include "basePass.hpp"
#include "glm/glm.hpp"

class Scene;
class RTVCollector;
class Primitive;

class BakerPass : public BasePass
{
public:
	explicit BakerPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~BakerPass() override = default;
	void bake(Scene* scene, uint32_t width, uint32_t height);
	void drawRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);
	void createOrResize();

private:
	uint32_t m_lastWidth = 0;
	uint32_t m_lastHeight = 0;

	ComPtr<ID3D11Buffer> m_constantBuffer;
	ComPtr<ID3D11Buffer> m_structuredVertexBuffer;
	ComPtr<ID3D11Buffer> m_structuredIndexBuffer;

	ComPtr<ID3D11ShaderResourceView> m_structuredVertexSRV;
	ComPtr<ID3D11ShaderResourceView> m_structuredIndexSRV;


	ComPtr<ID3D11Texture2D> m_worldSpaceTexelPositionTexture;
	ComPtr<ID3D11ShaderResourceView> m_worldSpaceTexelPositionSRV;
	ComPtr<ID3D11UnorderedAccessView> m_worldSpaceTexelPositionUAV;

	ComPtr<ID3D11Texture2D> m_worldSpaceTexelNormalTexture;
	ComPtr<ID3D11ShaderResourceView> m_worldSpaceTexelNormalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_worldSpaceTexelNormalUAV;


	ComPtr<ID3D11Texture2D> m_raycastVisualizationTexture;
	ComPtr<ID3D11ShaderResourceView> m_raycastVisualizationSRV;
	ComPtr<ID3D11RenderTargetView> m_raycastVisualizationRTV;

	ComPtr<ID3D11Texture2D> m_raycastDepthTexture;
	ComPtr<ID3D11DepthStencilView> m_raycastDepthDSV;
	ComPtr<ID3D11DepthStencilState> m_raycastDepthStencilState;
	ComPtr<ID3D11RasterizerState> m_raycastRasterizerState;
	ComPtr<ID3D11Buffer> m_raycastConstantBuffer;


	void updateBake(uint32_t width, uint32_t height, Primitive* prim);
	void updateRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);

	void createOrResizeBakedResources(uint32_t width, uint32_t height, Primitive* prim);

	std::unique_ptr<RTVCollector> m_rtvCollector;
};