#pragma once

#include <memory>
#include <string>

#include <d3d11_4.h>
#include <glm/glm.hpp>
#include <wrl.h>

#include "basePass.hpp"
#include "texture.hpp"

using namespace Microsoft::WRL;

class RTVCollector;

class CubeMapPass : public BasePass
{
public:
	CubeMapPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, const std::string& hdrImagePath);
	~CubeMapPass() override = default;

	void createOrResize();
	void draw(glm::mat4& view, glm::mat4& projection);
	ComPtr<ID3D11ShaderResourceView> getBackgroundSRV();
	ComPtr<ID3D11ShaderResourceView> getIrradianceSRV();
	ComPtr<ID3D11ShaderResourceView> getPrefilteredSRV();
	ComPtr<ID3D11ShaderResourceView> getBRDFLutSRV();
	std::string& getHDRIPath();

private:
	void update(const glm::mat4& view, const glm::mat4& projection) const;
	void createCubeMapResources();
	void createBackgroundResources();
	void createIrradianceMap();
	void createPrefilteredMap();
	void createBRDFLut();
	void processEnvironmentRotation();

	std::string m_hdrImagePath;
	std::unique_ptr<Texture> m_hdriTexture;
	std::unique_ptr<RTVCollector> m_rtvCollector;

	ComPtr<ID3D11Buffer> m_backgroundConstantBuffer;
	ComPtr<ID3D11Buffer> m_equirectToCubemapConstantBuffer;
	ComPtr<ID3D11Buffer> m_vertexBuffer;

	ComPtr<ID3D11Texture2D> m_CubeMapTexture;
	ComPtr<ID3D11UnorderedAccessView> m_CubeMapUAV;
	ComPtr<ID3D11ShaderResourceView> m_CubeMapSRV;

	ComPtr<ID3D11Texture2D> m_backgroundTexture;
	ComPtr<ID3D11RenderTargetView> m_backgroundRTV;
	ComPtr<ID3D11ShaderResourceView> m_backgroundSRV;

	// IBL resources
	ComPtr<ID3D11Texture2D> m_irradianceTexture;
	ComPtr<ID3D11UnorderedAccessView> m_irradianceUAV;
	ComPtr<ID3D11ShaderResourceView> m_irradianceSRV;

	ComPtr<ID3D11Texture2D> m_prefilteredTexture;
	ComPtr<ID3D11UnorderedAccessView> m_prefilteredUAV;
	ComPtr<ID3D11ShaderResourceView> m_prefilteredSRV;

	ComPtr<ID3D11Texture2D> m_brdfLutTexture;
	ComPtr<ID3D11UnorderedAccessView> m_brdfLutUAV;
	ComPtr<ID3D11ShaderResourceView> m_brdfLutSRV;

	ComPtr<ID3D11InputLayout> m_inputLayout;
};
