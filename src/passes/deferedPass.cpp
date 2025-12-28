#include "deferedPass.hpp"

#include <iostream>

#include "appConfig.hpp"
#include "GBufferTextures.hpp"
#include "debugPassMacros.hpp"
#include "shaderManager.hpp"
#include "scene.hpp"
#include "light.hpp"

static constexpr UINT COMPUTE_THREAD_GROUP_SIZE = 16;
static constexpr UINT MAX_LIGHTS = 100;

struct alignas(16) DeferredConstantBuffer
{
	float IBLrotationY;
	float IBLintensity;
	int32_t selectedID;
	float backgroundIntensity; // Align to 16 bytes
	float cameraPosition[3];
	float padding2; // Padding to align to 16 bytes
};

DeferredPass::DeferredPass(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context)
	: m_device(device)
	, m_context(context)
{
	m_shaderManager = std::make_unique<ShaderManager>(m_device);
	createOrResize();
	m_shaderManager->LoadComputeShader("deferred", L"../../src/shaders/deferred.hlsl", "CS");
	D3D11_BUFFER_DESC lightsBufferDesc = {};
	lightsBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	lightsBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightsBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightsBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	lightsBufferDesc.ByteWidth = sizeof(LightData) * MAX_LIGHTS; // Allocate space for up to 100 lights
	lightsBufferDesc.StructureByteStride = sizeof(LightData);

	{
		HRESULT hr = m_device->CreateBuffer(&lightsBufferDesc, nullptr, &m_lightsBuffer);
		if (FAILED(hr))
			std::cerr << "Error Creating Lights Buffer: " << hr << std::endl;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MAX_LIGHTS; // Match the buffer size

	{
		HRESULT hr = m_device->CreateShaderResourceView(m_lightsBuffer.Get(), &srvDesc, &m_lightsSRV);
		if (FAILED(hr))
			std::cerr << "Error Creating Lights SRV: " << hr << std::endl;
	}

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	{
		HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
		if (FAILED(hr))
			std::cerr << "Error Creating Sampler State: " << hr << std::endl;
	}

	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.ByteWidth = sizeof(DeferredConstantBuffer);
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	{
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);
		if (FAILED(hr))
			std::cerr << "Error Creating Constant Buffer: " << hr << std::endl;
	}
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
void DeferredPass::draw(const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& cameraPosition,
	Scene* scene,
	const GBufferTextures& gbufferTextures,
	const ComPtr<ID3D11ShaderResourceView>& depthSRV,
	const ComPtr<ID3D11ShaderResourceView>& backgroundSRV,
	const ComPtr<ID3D11ShaderResourceView>& irradianceSRV,
	const ComPtr<ID3D11ShaderResourceView>& prefilteredSRV,
	const ComPtr<ID3D11ShaderResourceView>& brdfLutSRV
)
{
	DEBUG_PASS_START(L"Deferred Pass Draw");

	if (AppConfig::getNeedsResize())
	{
		createOrResize();
	}
	if (scene->areLightsDirty())
	{
		updateLights(scene);
		scene->setLightsDirty(false);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		DeferredConstantBuffer* cbData = static_cast<DeferredConstantBuffer*>(mappedResource.pData);
		cbData->IBLrotationY = AppConfig::getIBLRotation();
		cbData->IBLintensity = AppConfig::getIBLIntensity();
		cbData->selectedID = scene->getActivePrimitiveID();
		cbData->backgroundIntensity = AppConfig::getBackgroundIntensity();
		cbData->cameraPosition[0] = cameraPosition.x;
		cbData->cameraPosition[1] = cameraPosition.y;
		cbData->cameraPosition[2] = cameraPosition.z;
		cbData->padding2 = 0.0f;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	m_context->CSSetShader(m_shaderManager->getComputeShader("deferred"), nullptr, 0);

	// Set SRVs
	ID3D11ShaderResourceView* srvs[] = {
		gbufferTextures.albedoSRV.Get(),
		gbufferTextures.metallicRoughnessSRV.Get(),
		gbufferTextures.normalSRV.Get(),
		gbufferTextures.positionSRV.Get(),
		gbufferTextures.objectIDSRV.Get(),
		depthSRV.Get(),
		m_lightsSRV.Get(),
		backgroundSRV.Get(),
		irradianceSRV.Get(),
		prefilteredSRV.Get(),
		brdfLutSRV.Get()
	};
	m_context->CSSetShaderResources(0, 11, srvs);
	m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	// Set UAVs
	ID3D11UnorderedAccessView* uavs[] = {
		m_finalUAV.Get()
	};
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	// Dispatch compute shader
	UINT dispatchX = static_cast<UINT>(std::ceil((AppConfig::getViewportWidth() + COMPUTE_THREAD_GROUP_SIZE - 1) / COMPUTE_THREAD_GROUP_SIZE));
	UINT dispatchY = static_cast<UINT>(std::ceil((AppConfig::getViewportHeight() + COMPUTE_THREAD_GROUP_SIZE - 1) / COMPUTE_THREAD_GROUP_SIZE));
	if (dispatchX == 0) dispatchX = 1;
	if (dispatchY == 0) dispatchY = 1;
	m_context->Dispatch(dispatchX, dispatchY, 1);

	// Unbind SRVs and UAVs
	ID3D11ShaderResourceView* nullSRVs[11] = { nullptr,nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	m_context->CSSetShaderResources(0, 11, nullSRVs);
	ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
	m_context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

	DEBUG_PASS_END();
}


ComPtr<ID3D11ShaderResourceView> DeferredPass::getFinalSRV() const
{
	return m_finalSRV;
}

ComPtr<ID3D11RenderTargetView> DeferredPass::getFinalRTV() const
{
	return m_finalRTV;
}

void DeferredPass::updateLights(Scene* scene)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	HRESULT hr = m_context->Map(m_lightsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* lightData = static_cast<LightData*>(mappedResource.pData);
		const auto& lights = scene->getLights();
	    int32_t i = 0;
	    for (auto& [handle, light] : lights)
	    {
			lightData[i] = light->getLightData();
	        ++i;
	    }
		m_context->Unmap(m_lightsBuffer.Get(), 0);
	}
}
