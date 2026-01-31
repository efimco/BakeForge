#include "pbrCubeMapPass.hpp"

#include "texture.hpp"

#include "appConfig.hpp"
#include "rtvCollector.hpp"
#include "shaderManager.hpp"

static D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0} };

auto cubeData = new float[] {
	// +X face
	1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
		// -X face
		-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f,
		1.0f,
		// +Y face
		-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f,
		// -Y face
		-1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
		1.0f,
		// +Z face
		-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,
		// -Z face
		1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
		-1.0f};

struct alignas(16) CubeMapConstantBufferData
{
	glm::mat4 viewProj;
	float mapRotationY;
	uint32_t isBlurred;
	float blurAmount;
	float padding;
};

struct alignas(16) EquirectToCubempConstantBufferData
{
	uint32_t faceSize;
	uint32_t _pad[3];
};

struct alignas(16) PrefilteredMapConstantBufferData
{
	uint32_t mipSize;
	uint32_t mipLevel;
	uint32_t _pad[2];
};


CubeMapPass::CubeMapPass(ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> context,
	const std::string& hdrImagePath)
	: BasePass(device, context), m_hdrImagePath(hdrImagePath)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_hdriTexture = std::make_unique<Texture>(m_hdrImagePath, m_device);

	m_shaderManager->LoadVertexShader("cubemapVS", L"../../src/shaders/cubemap.hlsl", "VS");
	m_shaderManager->LoadPixelShader("cubemapPS", L"../../src/shaders/cubemap.hlsl", "PS");
	m_shaderManager->LoadComputeShader("equirectToCubempCS", L"../../src/shaders/equirectToCubemp.hlsl", "CS");
	m_shaderManager->LoadComputeShader("irradianceMapCS", L"../../src/shaders/irradianceConvolution.hlsl", "CS");
	m_shaderManager->LoadComputeShader("prefilteredMapCS", L"../../src/shaders/prefilteredMap.hlsl", "CS");
	m_shaderManager->LoadComputeShader("brdfLUTCS", L"../../src/shaders/LUT4BRDF.hlsl", "CS");

	m_samplerState = createSamplerState(SamplerPreset::LinearClamp);

	D3D11_BUFFER_DESC constantBufferDesc;
	constantBufferDesc.ByteWidth = sizeof(CubeMapConstantBufferData);
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.StructureByteStride = 0;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	{
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_backgroundConstantBuffer);
		assert(SUCCEEDED(hr));
	}

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(float) * 3 * 36; // 36 vertices for a cube
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexSubresourceData;
	vertexSubresourceData.pSysMem = cubeData;
	vertexSubresourceData.SysMemPitch = 0;
	vertexSubresourceData.SysMemSlicePitch = 0;

	{
		HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &m_vertexBuffer);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create cube vertex buffer.");
		}
	}

	{
		HRESULT hr = m_device->CreateInputLayout(
			inputLayoutDesc, 1, m_shaderManager->getVertexShaderBlob("cubemapVS")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("cubemapVS")->GetBufferSize(), &m_inputLayout);
		assert(SUCCEEDED(hr));
	}

	m_rasterizerState = createRSState(RasterizerPreset::BackCull);
	m_depthStencilState = createDSState(DepthStencilPreset::ReadOnlyLessEqual);

	createCubeMapResources();
	createIrradianceMap();
	createPrefilteredMap();
	createBRDFLut();
}

void CubeMapPass::createOrResize()
{
	createBackgroundResources();
}

void CubeMapPass::draw(glm::mat4& view, glm::mat4& projection)
{

	beginDebugEvent(L"CubemapBGDrawPass");
	if (AppConfig::regeneratePrefilteredMap)
	{
		createPrefilteredMap();
		createBRDFLut();
		AppConfig::regeneratePrefilteredMap = false;
	}
	setViewport(AppConfig::viewportWidth, AppConfig::viewportHeight);
	update(view, projection);
	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_context->OMSetRenderTargets(1, m_backgroundRTV.GetAddressOf(), nullptr);

	constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_context->ClearRenderTargetView(m_backgroundRTV.Get(), clearColor);

	// Set vertex buffer
	constexpr UINT stride = sizeof(float) * 3;
	constexpr UINT offset = 0;
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_context->VSSetConstantBuffers(0, 1, m_backgroundConstantBuffer.GetAddressOf());
	m_context->PSSetConstantBuffers(0, 1, m_backgroundConstantBuffer.GetAddressOf());
	m_context->VSSetConstantBuffers(0, 1, m_backgroundConstantBuffer.GetAddressOf());
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_context->VSSetShader(m_shaderManager->getVertexShader("cubemapVS"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("cubemapPS"), nullptr, 0);
	m_context->PSSetShaderResources(0, 1, m_prefilteredSRV.GetAddressOf());
	m_context->PSSetShaderResources(1, 1, m_hdriTexture->srv.GetAddressOf());
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	// Draw the cube
	m_context->Draw(36, 0); // 36 vertices for a cube
	unbindRenderTargets(1);
	unbindShaderResources(0, 2);
	endDebugEvent();
}

ComPtr<ID3D11ShaderResourceView> CubeMapPass::getBackgroundSRV()
{
	return m_backgroundSRV;
}

ComPtr<ID3D11ShaderResourceView> CubeMapPass::getIrradianceSRV()
{
	return m_irradianceSRV;
}

ComPtr<ID3D11ShaderResourceView> CubeMapPass::getPrefilteredSRV()
{
	return m_prefilteredSRV;
}

ComPtr<ID3D11ShaderResourceView> CubeMapPass::getBRDFLutSRV()
{
	return m_brdfLutSRV;
}

std::string& CubeMapPass::getHDRIPath()
{
	return m_hdrImagePath;
}

void CubeMapPass::update(const glm::mat4& view, const glm::mat4& projection) const
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_backgroundConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to map constant buffer.");
	}
	const auto data = static_cast<CubeMapConstantBufferData*>(mappedResource.pData);
	// Remove translation from view matrix so skybox stays centered on camera
	glm::mat4 viewNoTranslation = view;
	viewNoTranslation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	data->viewProj = glm::transpose(projection * viewNoTranslation);
	data->mapRotationY = AppConfig::IBLrotation;
	data->isBlurred = AppConfig::isBackgroundBlurred ? 1 : 0;
	data->blurAmount = AppConfig::isBackgroundBlurred ? AppConfig::blurAmount : 0.0f;
	m_context->Unmap(m_backgroundConstantBuffer.Get(), 0);
}

void CubeMapPass::createCubeMapResources()
{
	// Create the cubemap texture
	uint32_t sideSize = (m_hdriTexture->getWidth() + m_hdriTexture->getHeight()) / 6;
	D3D11_TEXTURE2D_DESC cubeMapDesc = {};
	cubeMapDesc.ArraySize = 6;
	cubeMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	cubeMapDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	cubeMapDesc.Height = sideSize;
	cubeMapDesc.Width = sideSize;
	cubeMapDesc.MipLevels = 1;
	cubeMapDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	cubeMapDesc.SampleDesc.Count = 1;
	cubeMapDesc.Usage = D3D11_USAGE_DEFAULT;
	{
		HRESULT hr = m_device->CreateTexture2D(&cubeMapDesc, nullptr, &m_CubeMapTexture);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create cubemap texture.");
		}
	}

	// Create the unordered access view for the cubemap
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
	uavd.Format = cubeMapDesc.Format;
	uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	uavd.Texture2DArray.MipSlice = 0;
	uavd.Texture2DArray.FirstArraySlice = 0;
	uavd.Texture2DArray.ArraySize = 6;
	{


		HRESULT hr = m_device->CreateUnorderedAccessView(m_CubeMapTexture.Get(), &uavd, &m_CubeMapUAV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create cubemap unordered access view.");
		}
	}

	// Create the shader resource view for the cubemap
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeMapDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = cubeMapDesc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;

	{
		HRESULT hr = m_device->CreateShaderResourceView(m_CubeMapTexture.Get(), &srvDesc, &m_CubeMapSRV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create cubemap shader resource view.");
		}
	}

	// Create the constant buffer
	{
		D3D11_BUFFER_DESC constantBufferDesc = {};
		constantBufferDesc.ByteWidth = sizeof(EquirectToCubempConstantBufferData);
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.StructureByteStride = 0;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;

		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_equirectToCubemapConstantBuffer);
		assert(SUCCEEDED(hr));
	}


	D3D11_MAPPED_SUBRESOURCE mappedResource;

	m_context->Map(m_equirectToCubemapConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	auto data = static_cast<EquirectToCubempConstantBufferData*>(mappedResource.pData);
	data->faceSize = sideSize;
	m_context->Unmap(m_equirectToCubemapConstantBuffer.Get(), 0);


	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	ComPtr<ID3D11SamplerState> samplerState;
	{
		HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &samplerState);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create sampler state.");
		}
	}

	m_context->CSSetSamplers(0, 1, samplerState.GetAddressOf());

	m_context->CSSetUnorderedAccessViews(0, 1, m_CubeMapUAV.GetAddressOf(), nullptr);
	m_context->CSSetConstantBuffers(0, 1, m_equirectToCubemapConstantBuffer.GetAddressOf());
	m_context->CSSetShaderResources(0, 1, m_hdriTexture->srv.GetAddressOf());
	m_context->CSSetShader(m_shaderManager->getComputeShader("equirectToCubempCS"), nullptr, 0);
	uint32_t gx = (sideSize + 7) / 8;
	uint32_t gy = (sideSize + 7) / 8;
	m_context->Dispatch(gx, gy, 6);
}

void CubeMapPass::createBackgroundResources()
{
	m_backgroundTexture = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

	m_backgroundRTV = createRenderTargetView(m_backgroundTexture.Get(), RTVPreset::Texture2D);
	m_backgroundSRV = createShaderResourceView(m_backgroundTexture.Get(), SRVPreset::Texture2D);
	m_rtvCollector->addRTV("CUBEMAP::background", m_backgroundSRV.Get());
}

void CubeMapPass::createIrradianceMap()
{
	beginDebugEvent(L"Irradiance Map");
	static constexpr uint32_t irradianceMapSize = 32;
	D3D11_TEXTURE2D_DESC irradianceDesc = {};
	irradianceDesc.ArraySize = 6; // 6 faces for cubemap
	irradianceDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	irradianceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	irradianceDesc.Width = irradianceMapSize;
	irradianceDesc.Height = irradianceMapSize;
	irradianceDesc.MipLevels = 1;
	irradianceDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	irradianceDesc.SampleDesc.Count = 1;
	irradianceDesc.Usage = D3D11_USAGE_DEFAULT;
	{
		HRESULT hr = m_device->CreateTexture2D(&irradianceDesc, nullptr, &m_irradianceTexture);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create irradiance texture.");
		}
	}
	// Create the unordered access view for the irradiance map
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
	uavd.Format = irradianceDesc.Format;
	uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	uavd.Texture2DArray.MipSlice = 0;
	uavd.Texture2DArray.FirstArraySlice = 0;
	uavd.Texture2DArray.ArraySize = 6;
	{
		HRESULT hr = m_device->CreateUnorderedAccessView(m_irradianceTexture.Get(), &uavd, &m_irradianceUAV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create irradiance unordered access view.");
		}
	}

	// Create the shader resource view for the irradiance map
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = irradianceDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = irradianceDesc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	{
		HRESULT hr = m_device->CreateShaderResourceView(m_irradianceTexture.Get(), &srvDesc, &m_irradianceSRV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create irradiance shader resource view.");
		}
	}

	// Create constant buffer for irradiance generation with correct structure
	ComPtr<ID3D11Buffer> irradianceConstantBuffer;
	{
		D3D11_BUFFER_DESC constantBufferDesc = {};
		constantBufferDesc.ByteWidth = sizeof(EquirectToCubempConstantBufferData); // Same structure works
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.StructureByteStride = 0;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;

		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &irradianceConstantBuffer);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create irradiance constant buffer.");
		}
	}

	// Update constant buffer
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(irradianceConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto data = static_cast<EquirectToCubempConstantBufferData*>(mappedResource.pData);
		data->faceSize = irradianceMapSize;
		m_context->Unmap(irradianceConstantBuffer.Get(), 0);
	}


	m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, m_irradianceUAV.GetAddressOf(), nullptr);
	m_context->CSSetShaderResources(0, 1, m_CubeMapSRV.GetAddressOf());
	m_context->CSSetConstantBuffers(0, 1, irradianceConstantBuffer.GetAddressOf());
	m_context->CSSetShader(m_shaderManager->getComputeShader("irradianceMapCS"), nullptr, 0);
	uint32_t gx = 8;
	uint32_t gy = gx;
	m_context->Dispatch(gx, gy, 6); // Dispatch 6 for all cubemap faces!

	// Unbind resources
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;
	m_context->CSSetShaderResources(0, 1, &nullSRV);
	m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	m_context->CSSetSamplers(0, 1, &nullSampler);
	endDebugEvent();
}

void CubeMapPass::createPrefilteredMap()
{
	beginDebugEvent(L"PrefilteredMap");
	if (m_prefilteredTexture)
	{
		// Release existing resources
		m_prefilteredTexture.Reset();
		m_prefilteredSRV.Reset();
	}

	constexpr uint32_t prefilteredMapSize = 512;
	constexpr uint32_t numMips = 9; // 0-8 mip levels to match shader

	D3D11_TEXTURE2D_DESC prefilteredMapDesc = {};
	prefilteredMapDesc.ArraySize = 6; // 6 faces for cubemap
	prefilteredMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	prefilteredMapDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	prefilteredMapDesc.Width = prefilteredMapSize;
	prefilteredMapDesc.Height = prefilteredMapSize;
	prefilteredMapDesc.MipLevels = numMips;
	prefilteredMapDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	prefilteredMapDesc.SampleDesc.Count = 1;
	prefilteredMapDesc.Usage = D3D11_USAGE_DEFAULT;
	{
		HRESULT hr = m_device->CreateTexture2D(&prefilteredMapDesc, nullptr, &m_prefilteredTexture);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create prefiltered texture.");
		}
	}

	// Create the shader resource view for the prefiltered map
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = prefilteredMapDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = prefilteredMapDesc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	{
		HRESULT hr = m_device->CreateShaderResourceView(m_prefilteredTexture.Get(), &srvDesc, &m_prefilteredSRV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create prefiltered shader resource view.");
		}
	}

	// Create constant buffer for prefiltered map generation
	ComPtr<ID3D11Buffer> prefilteredConstantBuffer;
	{
		D3D11_BUFFER_DESC constantBufferDesc = {};
		constantBufferDesc.ByteWidth = sizeof(PrefilteredMapConstantBufferData);
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.StructureByteStride = 0;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;

		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &prefilteredConstantBuffer);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create prefiltered constant buffer.");
		}
	}


	// Generate each mip level separately
	for (uint32_t mip = 0; mip < numMips; ++mip)
	{
		// Calculate mip size
		uint32_t currentMipSize = std::max(1u, prefilteredMapSize >> mip);

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = m_context->Map(prefilteredConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr))
		{
			auto data = static_cast<PrefilteredMapConstantBufferData*>(mappedResource.pData);
			data->mipSize = currentMipSize;
			data->mipLevel = mip;
			m_context->Unmap(prefilteredConstantBuffer.Get(), 0);
		}

		// Create UAV for this specific mip level
		ComPtr<ID3D11UnorderedAccessView> mipUAV;
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = prefilteredMapDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mip;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = 6;

		hr = m_device->CreateUnorderedAccessView(m_prefilteredTexture.Get(), &uavDesc, &mipUAV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create prefiltered UAV for mip level.");
		}

		// Set compute shader resources
		m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());
		m_context->CSSetUnorderedAccessViews(0, 1, mipUAV.GetAddressOf(), nullptr);
		m_context->CSSetShaderResources(0, 1, m_CubeMapSRV.GetAddressOf());
		m_context->CSSetConstantBuffers(0, 1, prefilteredConstantBuffer.GetAddressOf());
		m_context->CSSetShader(m_shaderManager->getComputeShader("prefilteredMapCS"), nullptr, 0);

		// Dispatch for this mip level (shader uses 32x32 threads)
		uint32_t gx = (currentMipSize + 31) / 32;
		uint32_t gy = (currentMipSize + 31) / 32;
		m_context->Dispatch(gx, gy, 1);

		// Unbind resources after each mip
		ID3D11ShaderResourceView* nullSRV = nullptr;
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		ID3D11SamplerState* nullSampler = nullptr;
		m_context->CSSetShaderResources(0, 1, &nullSRV);
		m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		m_context->CSSetSamplers(0, 1, &nullSampler);
	}
	endDebugEvent();
}

void CubeMapPass::createBRDFLut()
{
	beginDebugEvent(L"BRDFLut");
	if (m_brdfLutTexture)
	{
		// Release existing resources
		m_brdfLutTexture.Reset();
		m_brdfLutSRV.Reset();
		m_brdfLutUAV.Reset();
	}
	constexpr uint32_t brdfLUTsize = 256;

	D3D11_TEXTURE2D_DESC brdfLUTDesc = {};
	brdfLUTDesc.ArraySize = 1;
	brdfLUTDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	brdfLUTDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	brdfLUTDesc.Width = brdfLUTsize;
	brdfLUTDesc.Height = brdfLUTsize;
	brdfLUTDesc.MipLevels = 1;
	brdfLUTDesc.MiscFlags = 0;
	brdfLUTDesc.SampleDesc.Count = 1;
	brdfLUTDesc.Usage = D3D11_USAGE_DEFAULT;
	{
		HRESULT hr = m_device->CreateTexture2D(&brdfLUTDesc, nullptr, &m_brdfLutTexture);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create BRDF LUT texture.");
		}
	}

	// Create the shader resource view for the BRDF LUT
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = brdfLUTDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = brdfLUTDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	{
		HRESULT hr = m_device->CreateShaderResourceView(m_brdfLutTexture.Get(), &srvDesc, &m_brdfLutSRV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create BRDF LUT shader resource view.");
		}
	}
	m_rtvCollector->addRTV("CUBEMAP::BRDF", m_brdfLutSRV.Get());

	// Create unordered access view for BRDF LUT
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = brdfLUTDesc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	{
		HRESULT hr = m_device->CreateUnorderedAccessView(m_brdfLutTexture.Get(), &uavDesc, &m_brdfLutUAV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create BRDF LUT unordered access view.");
		}
	}


	// Generate BRDF LUT
	m_context->CSSetUnorderedAccessViews(0, 1, m_brdfLutUAV.GetAddressOf(), nullptr);
	m_context->CSSetShader(m_shaderManager->getComputeShader("brdfLUTCS"), nullptr, 0);
	uint32_t gx = 32;
	uint32_t gy = gx;
	m_context->Dispatch(gx, gy, 1);
	// Unbind resources
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

	endDebugEvent();
}
