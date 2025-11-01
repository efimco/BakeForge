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
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);
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
	createOrResize();
}

void CubeMapPass::createOrResize()
{
	createCubeMapResources();
	createBackgroundResources();

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

std::string& CubeMapPass::getHDRIPath()
{
	return m_hdrImagePath;
}

void CubeMapPass::update(glm::mat4& view, glm::mat4& projection, glm::vec3& cameraPosition)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to map constant buffer.");
	}
	CubeMapConstantBufferData* data = reinterpret_cast<CubeMapConstantBufferData*>(mappedResource.pData);
	data->view = view;
	data->projection = projection;
	data->cameraPosition = cameraPosition;
	data->padding = 0.0f;
	m_context->Unmap(m_constantBuffer.Get(), 0);
}

void CubeMapPass::createCubeMapResources()
{
	// Create the cubemap texture
	D3D11_TEXTURE2D_DESC cubeMapDesc = {};
	cubeMapDesc.ArraySize = 6;
	cubeMapDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	cubeMapDesc.CPUAccessFlags = 0;
	cubeMapDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	cubeMapDesc.Height = 512; // Example size, adjust as needed
	cubeMapDesc.Width = 512;  // Example size, adjust as needed
	cubeMapDesc.MipLevels = 1;
	cubeMapDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	cubeMapDesc.SampleDesc.Count = 1;
	cubeMapDesc.SampleDesc.Quality = 0;
	cubeMapDesc.Usage = D3D11_USAGE_DEFAULT;

	HRESULT hr = m_device->CreateTexture2D(&cubeMapDesc, nullptr, &m_CubeMapTexture);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create cubemap texture.");
	}

	// Create the render target view for the cubemap
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = cubeMapDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.FirstArraySlice = 0;
	rtvDesc.Texture2DArray.MipSlice = 0;

	hr = m_device->CreateRenderTargetView(m_CubeMapTexture.Get(), &rtvDesc, &m_CubeMapRTV);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create cubemap render target view.");
	}

	// Create the shader resource view for the cubemap
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeMapDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = cubeMapDesc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;

	{
		hr = m_device->CreateShaderResourceView(m_CubeMapTexture.Get(), &srvDesc, &m_CubeMapSRV);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create cubemap shader resource view.");
		}
	}
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
