#pragma once
#include <d3d11.h>
#include <memory>
#include "basePass.hpp"

#include "scene.hpp"

class RTVCollector;


class BVHDebugPass : public BasePass
{
public:
	explicit BVHDebugPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	virtual ~BVHDebugPass() = default;
	void draw(Scene* scene, const glm::mat4& view, const glm::mat4& proj);
	void createOrResize();

private:
	void update(const glm::mat4& view, const glm::mat4& proj);
	void updateBvhNodesBuffer(Primitive* prim);

	std::unique_ptr<RTVCollector> m_rtvCollector;

	ComPtr<ID3D11InputLayout> m_inputLayout;

	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;

	ComPtr<ID3D11Buffer> m_constantBuffer;

	ComPtr<ID3D11Buffer> m_bvhNodesBuffer; // structuredBuffer
	ComPtr<ID3D11ShaderResourceView> m_bvhNodesSrv;


	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;

	ComPtr<ID3D11Texture2D> m_depthTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	ComPtr<ID3D11BlendState> m_blendState;
	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
};
