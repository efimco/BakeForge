#include "deferedPass.hpp"
#include "appConfig.hpp"
#include <iostream>
#include "debugPassMacros.hpp"

DeferredPass::DeferredPass(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context)
	: m_device(device), m_context(context)
{
	m_shaderManager = std::make_unique<ShaderManager>(m_device);
	createOrResize();
	m_shaderManager->LoadComputeShader("deferred", L"../../src/shaders/deferred.hlsl", "CS");

}

void DeferredPass::createOrResize()
{
	if (m_finalTexture != nullptr)
	{
		m_finalTexture.Reset();
		m_finalRTV.Reset();
		m_finalSRV.Reset();
	}

	// Create final render target texture, RTV, and SRV
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Height = AppConfig::getViewportHeight();
	textureDesc.Width = AppConfig::getViewportWidth();
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	// Create Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

	// Create Unordered Access View
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = textureDesc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	{
		HRESULT hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_finalTexture);
		if (FAILED(hr))
			std::cerr << "Error Creating Final Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(m_finalTexture.Get(), nullptr, &m_finalRTV);
		if (FAILED(hr))
			std::cerr << "Error Creating Final RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(m_finalTexture.Get(), &srvDesc, &m_finalSRV);
		if (FAILED(hr))
			std::cerr << "Error Creating Final SRV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateUnorderedAccessView(m_finalTexture.Get(), &uavDesc, &m_finalUAV);
		if (FAILED(hr))
			std::cerr << "Error Creating Final UAV: " << hr << std::endl;
	}
}
void DeferredPass::draw(const glm::mat4& view, const glm::mat4& projection,
	const ComPtr<ID3D11ShaderResourceView>& albedoSRV,
	const ComPtr<ID3D11ShaderResourceView>& metallicRoughnessSRV,
	const ComPtr<ID3D11ShaderResourceView>& normalSRV,
	const ComPtr<ID3D11ShaderResourceView>& positionSRV,
	const ComPtr<ID3D11ShaderResourceView>& objectIDSRV,
	const ComPtr<ID3D11ShaderResourceView>& depthSRV
)
{
	DEBUG_PASS_START(L"Deferred Pass Draw");

	if (AppConfig::getNeedsResize())
	{
		createOrResize();
	}

	m_context->CSSetShader(m_shaderManager->getComputeShader("deferred"), nullptr, 0);

	// Set SRVs
	ID3D11ShaderResourceView* srvs[] = {
		albedoSRV.Get(),
		metallicRoughnessSRV.Get(),
		normalSRV.Get(),
		positionSRV.Get(),
		objectIDSRV.Get(),
		depthSRV.Get()
	};
	m_context->CSSetShaderResources(0, 6, srvs);

	// Set UAVs
	ID3D11UnorderedAccessView* uavs[] = {
		m_finalUAV.Get()
	};
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	// Dispatch compute shader
	UINT dispatchX = static_cast<UINT>(std::ceil(AppConfig::getViewportWidth() / 16.0f));
	UINT dispatchY = static_cast<UINT>(std::ceil(AppConfig::getViewportHeight() / 16.0f));
	m_context->Dispatch(dispatchX, dispatchY, 1);

	// Unbind SRVs and UAVs
	ID3D11ShaderResourceView* nullSRVs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	m_context->CSSetShaderResources(0, 5, nullSRVs);
	ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
	m_context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

	DEBUG_PASS_END();
}


ComPtr<ID3D11ShaderResourceView> DeferredPass::getFinalSRV() const
{
	return m_finalSRV;
}
