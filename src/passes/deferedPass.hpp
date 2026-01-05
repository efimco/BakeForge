#pragma once

#include <memory>

#include <d3d11_4.h>
#include <wrl.h>

#include <glm/glm.hpp>

#include "basePass.hpp"

class Scene;
class ShaderManager;
struct GBufferTextures;

using namespace Microsoft::WRL;
class DeferredPass : public BasePass
{
public:
	DeferredPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~DeferredPass() = default;

	void createOrResize();
	void draw(const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPosition,
		Scene* scene,
		const GBufferTextures& gbufferTextures,
		ComPtr<ID3D11ShaderResourceView> depthSRV,
		ComPtr<ID3D11ShaderResourceView> backgroundSRV,
		ComPtr<ID3D11ShaderResourceView> irradianceSRV,
		ComPtr<ID3D11ShaderResourceView> prefilteredSRV,
		ComPtr<ID3D11ShaderResourceView> brdfLutSRV,
		ComPtr<ID3D11ShaderResourceView> worldSpaceUISRV
	);

	ComPtr<ID3D11ShaderResourceView> getFinalSRV() const;
	ComPtr<ID3D11RenderTargetView> getFinalRTV() const;
private:
	void updateLights(Scene* scene);

	ComPtr<ID3D11Texture2D> m_finalTexture;
	ComPtr<ID3D11RenderTargetView> m_finalRTV;
	ComPtr<ID3D11ShaderResourceView> m_finalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_finalUAV;

	ComPtr<ID3D11Buffer> m_lightsBuffer;
	ComPtr<ID3D11ShaderResourceView> m_lightsSRV;

	ComPtr<ID3D11Buffer> m_constantBuffer;
};