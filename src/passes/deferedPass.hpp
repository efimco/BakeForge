#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <glm/glm.hpp>
#include <memory>
#include "shaderManager.hpp"

using namespace Microsoft::WRL;
class DeferredPass
{
public:
	DeferredPass(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context);
	~DeferredPass() = default;

	void createOrResize();
	void draw(const glm::mat4& view, const glm::mat4& projection,
		const ComPtr<ID3D11ShaderResourceView>& albedoSRV,
		const ComPtr<ID3D11ShaderResourceView>& metallicRoughnessSRV,
		const ComPtr<ID3D11ShaderResourceView>& normalSRV,
		const ComPtr<ID3D11ShaderResourceView>& positionSRV,
		const ComPtr<ID3D11ShaderResourceView>& objectIDSRV,
		const ComPtr<ID3D11ShaderResourceView>& depthSRV,
		const ComPtr<ID3D11ShaderResourceView>& backgroundSRV,
		const ComPtr<ID3D11ShaderResourceView>& irradianceSRV,
		const ComPtr<ID3D11ShaderResourceView>& prefilteredSRV
	);

	ComPtr<ID3D11ShaderResourceView> getFinalSRV() const;
private:
	void update();
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11Texture2D> m_finalTexture;
	ComPtr<ID3D11RenderTargetView> m_finalRTV;
	ComPtr<ID3D11ShaderResourceView> m_finalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_finalUAV;

	ComPtr<ID3D11BlendState> m_blendState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11SamplerState> m_samplerState;

	std::unique_ptr<ShaderManager> m_shaderManager;
	ComPtr<ID3D11Buffer> m_lightsBuffer;
	ComPtr<ID3D11ShaderResourceView> m_lightsSRV;
};