#include "ZPrePass.hpp"
#include "appConfig.hpp"
#include <iostream>
#include "sceneManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "debugPassMacros.hpp"

struct alignas(16) ConstantBufferData
{
	glm::mat4 modelViewProjection;
};


ZPrePass::ZPrePass(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: m_device(device), m_context(context)
{
	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.AntialiasedLineEnable = false;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.ScissorEnable = false;

	{
		HRESULT hr = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState);
		if (FAILED(hr))
			std::cerr << "Error Creating Rasterizer State: " << hr << std::endl;
	}

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	{
		HRESULT hr = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
		if (FAILED(hr))
			std::cerr << "Error Creating DepthStencil State: " << hr << std::endl;
	}

	createOrResize();

	m_shaderManager = new ShaderManager(device);
	m_shaderManager->LoadVertexShader("zPrePass", L"../../src/shaders/ZPrePass.hlsl", "VS");

	{
		HRESULT hr = m_device->CreateInputLayout(
			zPrePassInputLayoutDesc,
			ARRAYSIZE(zPrePassInputLayoutDesc),
			m_shaderManager->getVertexShaderBlob("zPrePass")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("zPrePass")->GetBufferSize(),
			&m_inputLayout);
		assert(SUCCEEDED(hr));
	};

	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.StructureByteStride = 0;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	{
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantbuffer);
		assert(SUCCEEDED(hr));
	}

}

ZPrePass::~ZPrePass()
{
	delete m_shaderManager;
}

void ZPrePass::draw(const glm::mat4& view, const glm::mat4& projection)
{
	DEBUG_PASS_START(L"ZPrePass Draw");
	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetRenderTargets(0, nullptr, dsv.Get());
	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

	m_context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_context->VSSetShader(m_shaderManager->getVertexShader("zPrePass"), nullptr, 0);
	m_context->PSSetShader(nullptr, nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());

	static const UINT stride = sizeof(InterleavedData);
	static const UINT offset = 0;

	for (int i = 0; i < SceneManager::getPrimitiveCount(); i++)
	{
		std::unique_ptr<Primitive>& prim = SceneManager::getPrimitives()[i];
		update(view, projection, prim);
		m_context->IASetVertexBuffers(0, 1, prim->getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(prim->getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		m_context->DrawIndexed(static_cast<UINT>(prim->getIndexData().size()), 0, 0);
	}
	m_context->VSSetShader(nullptr, nullptr, 0);
	m_context->PSSetShader(nullptr, nullptr, 0);
	DEBUG_PASS_END();
}

ComPtr<ID3D11DepthStencilView>& ZPrePass::getDSV()
{
	return dsv;
}

void ZPrePass::update(const glm::mat4& view, const glm::mat4& projection, std::unique_ptr<Primitive>& prim)
{
	glm::mat4 mvp = projection * view * prim->transform.getWorldMatrix();
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		ConstantBufferData* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
		cbData->modelViewProjection = glm::transpose(mvp);
		m_context->Unmap(m_constantbuffer.Get(), 0);
	}
}

void ZPrePass::createOrResize()
{
	if (t_depth != nullptr)
	{
		t_depth.Reset();
		dsv.Reset();
		srv_depth.Reset();
	}

	//depth
	D3D11_TEXTURE2D_DESC depthDesc;
	depthDesc.ArraySize = 1;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthDesc.Height = AppConfig::getViewportHeight();
	depthDesc.Width = AppConfig::getViewportWidth();
	depthDesc.MipLevels = 1;
	depthDesc.MiscFlags = 0;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = 0;
	dsvDesc.Texture2D.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
	depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Texture2D.MostDetailedMip = 0;
	depthSRVDesc.Texture2D.MipLevels = depthDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&depthDesc, nullptr, &t_depth);
		if (FAILED(hr))
			std::cerr << "Error Creating DepthStencil Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateDepthStencilView(t_depth.Get(), &dsvDesc, &dsv);
		if (FAILED(hr))
			std::cerr << "Error Creating DepthStencil DSV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_depth.Get(), &depthSRVDesc, &srv_depth);
		if (FAILED(hr))
			std::cerr << "Error Creating DepthStencil SRV: " << hr << std::endl;
	}
}
