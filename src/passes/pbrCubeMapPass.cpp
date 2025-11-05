#include "pbrCubeMapPass.hpp"
#include "texture.hpp"
#include "iostream"
#include "glm/gtc/matrix_transform.hpp"
#include "debugPassMacros.hpp"
#include "appConfig.hpp"

float* cubeData = new float[]
	{
		// +X face
		1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			// -X face
			-1.0f, -1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, 1.0f,
			// +Y face
			-1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, -1.0f,
			// -Y face
			-1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			// +Z face
			-1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			-1.0f, -1.0f, 1.0f,
			// -Z face
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f
	};

struct alignas(16) CubeMapConstantBufferData
{
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec3 cameraPosition;
	float padding; // Align to 16 bytes
};

struct alignas(16) EquirectToCubempConstantBufferData
{
	uint32_t faceSize;
	uint32_t _pad[3];
};


CubeMapPass::CubeMapPass(ComPtr<ID3D11Device>& device,
	ComPtr<ID3D11DeviceContext>& context,
	std::string hdrImagePath) :
	m_device(device),
	m_context(context),
	m_hdrImagePath(hdrImagePath)
{

	m_hdriTexture = std::make_unique<Texture>(m_hdrImagePath, m_device, true);
	m_shaderManager = std::make_unique<ShaderManager>(device);
	m_shaderManager->LoadVertexShader("cubemapVS", L"../../src/shaders/cubemap.hlsl", "VS");
	m_shaderManager->LoadPixelShader("cubemapPS", L"../../src/shaders/cubemap.hlsl", "PS");
	m_shaderManager->LoadComputeShader("equirectToCubempCS", L"../../src/shaders/equirectToCubemp.hlsl", "CS");
	m_shaderManager->LoadComputeShader("irradianceMapCS", L"../../src/shaders/irradianceConvolution.hlsl", "CS");

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = samplerDesc.AddressU;
	samplerDesc.AddressW = samplerDesc.AddressU;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create sampler state.");
	}
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
	createCubeMapResources();
	createIrradianceMap();
	createPrefilteredMap();
	createBRDFLut();
}

void CubeMapPass::createOrResize()
{
	createBackgroundResources();
	createBRDFLut();
}
void CubeMapPass::draw(glm::mat4& view, glm::mat4& projection, glm::vec3& cameraPosition)
{
	DEBUG_PASS_START(L"CubeMapPass::draw");
	update(view, projection, cameraPosition);
	m_context->OMSetRenderTargets(1, m_backgroundRTV.GetAddressOf(), nullptr);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_context->ClearRenderTargetView(m_backgroundRTV.Get(), clearColor);

	// Set vertex buffer
	UINT stride = sizeof(float) * 3;
	UINT offset = 0;
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

	m_context->VSSetShader(m_shaderManager->getVertexShader("cubemapVS"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("cubemapPS"), nullptr, 0);

	m_context->PSSetShaderResources(0, 1, m_hdriTexture->srv.GetAddressOf());
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	// Draw the cube
	m_context->Draw(36, 0); // 36 vertices for a cube
	m_context->OMSetRenderTargets(0, nullptr, nullptr);
	DEBUG_PASS_END();

}

ComPtr<ID3D11ShaderResourceView>& CubeMapPass::getBackgroundSRV()
{
	return m_backgroundSRV;
}

ComPtr<ID3D11ShaderResourceView>& CubeMapPass::getIrradianceSRV()
{
	return m_irradianceSRV;
}

std::string& CubeMapPass::getHDRIPath()
{
	return m_hdrImagePath;
}

void CubeMapPass::update(glm::mat4& view, glm::mat4& projection, glm::vec3& cameraPosition)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_backgroundConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to map constant buffer.");
	}
	CubeMapConstantBufferData* data = reinterpret_cast<CubeMapConstantBufferData*>(mappedResource.pData);
	data->view = view;
	data->projection = projection;
	data->cameraPosition = cameraPosition;
	data->padding = 0.0f;
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
	EquirectToCubempConstantBufferData* data = reinterpret_cast<EquirectToCubempConstantBufferData*>(mappedResource.pData);
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
	DEBUG_PASS_START(L"CubeMapGeneration");
	m_context->CSSetSamplers(0, 1, samplerState.GetAddressOf());

	m_context->CSSetUnorderedAccessViews(0, 1, m_CubeMapUAV.GetAddressOf(), nullptr);
	m_context->CSSetConstantBuffers(0, 1, m_equirectToCubemapConstantBuffer.GetAddressOf());
	m_context->CSSetShaderResources(0, 1, m_hdriTexture->srv.GetAddressOf());
	m_context->CSSetShader(m_shaderManager->getComputeShader("equirectToCubempCS"), nullptr, 0);
	uint32_t gx = (sideSize + 7) / 8;
	uint32_t gy = (sideSize + 7) / 8;
	m_context->Dispatch(gx, gy, 6);
	DEBUG_PASS_END();
}

void CubeMapPass::createBackgroundResources()
{
	D3D11_TEXTURE2D_DESC backgroundDesc = {};
	backgroundDesc.ArraySize = 1;
	backgroundDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	backgroundDesc.CPUAccessFlags = 0;
	backgroundDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	backgroundDesc.Height = AppConfig::getViewportHeight();
	backgroundDesc.Width = AppConfig::getViewportWidth();
	backgroundDesc.MipLevels = 1;
	backgroundDesc.MiscFlags = 0;
	backgroundDesc.SampleDesc.Count = 1;
	backgroundDesc.SampleDesc.Quality = 0;
	backgroundDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC backgroundSRVDesc = {};
	backgroundSRVDesc.Format = backgroundDesc.Format;
	backgroundSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	backgroundSRVDesc.Texture2D.MostDetailedMip = 0;
	backgroundSRVDesc.Texture2D.MipLevels = backgroundDesc.MipLevels;

	D3D11_RENDER_TARGET_VIEW_DESC backgroundRTVDesc = {};
	backgroundRTVDesc.Format = backgroundDesc.Format;
	backgroundRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	backgroundRTVDesc.Texture2D.MipSlice = 0;


	HRESULT hr = m_device->CreateTexture2D(&backgroundDesc, nullptr, &m_backgroundTexture);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create background texture.");
	}

	hr = m_device->CreateRenderTargetView(m_backgroundTexture.Get(), nullptr, &m_backgroundRTV);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create background render target view.");
	}
	hr = m_device->CreateShaderResourceView(m_backgroundTexture.Get(), nullptr, &m_backgroundSRV);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create background shader resource view.");
	}
}

void CubeMapPass::createIrradianceMap()
{

	static const uint32_t irradianceMapSize = 128;
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

	DEBUG_PASS_START(L"IrradianceMapGeneration");
	m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, m_irradianceUAV.GetAddressOf(), nullptr);
	m_context->CSSetShaderResources(0, 1, m_CubeMapSRV.GetAddressOf());
	m_context->CSSetConstantBuffers(0, 1, m_equirectToCubemapConstantBuffer.GetAddressOf());
	m_context->CSSetShader(m_shaderManager->getComputeShader("irradianceMapCS"), nullptr, 0);
	uint32_t gx = (irradianceMapSize + 7) / 8;
	uint32_t gy = (irradianceMapSize + 7) / 8;
	m_context->Dispatch(gx, gy, 1);
	
	// Unbind resources
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;
	m_context->CSSetShaderResources(0, 1, &nullSRV);
	m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	m_context->CSSetSamplers(0, 1, &nullSampler);
	DEBUG_PASS_END();

}

void CubeMapPass::createPrefilteredMap()
{
}

void CubeMapPass::createBRDFLut()
{
}
