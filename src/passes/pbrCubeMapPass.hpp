#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <string>
#include "Texture.hpp"
#include <memory>
#include "shaderManager.hpp"
#include "glm/glm.hpp"

using namespace Microsoft::WRL;


class CubeMapPass
{
public:
	CubeMapPass(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context, std::string hdrImagePath);
	~CubeMapPass() = default;

	void createOrResize();
	void draw(glm::mat4& view, glm::mat4& projection, glm::vec3& cameraPosition);
	ComPtr<ID3D11ShaderResourceView>& getBackgroundSRV();
	ComPtr<ID3D11ShaderResourceView>& getIrradianceSRV();
	ComPtr<ID3D11ShaderResourceView>& getPrefilteredSRV();
	std::string& getHDRIPath();
	
private:
	void update(glm::mat4& view, glm::mat4& projection, glm::vec3& cameraPosition);
	void createCubeMapResources();
	void createBackgroundResources();
	void createIrradianceMap();
	void createPrefilteredMap();
	void createBRDFLut();
	std::string m_hdrImagePath;
	std::unique_ptr<ShaderManager> m_shaderManager;
	std::unique_ptr<Texture> m_hdriTexture;

	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11Buffer> m_backgroundConstantBuffer;
	ComPtr<ID3D11Buffer> m_equirectToCubemapConstantBuffer;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11DeviceContext> m_context;

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

};
