#include "basePass.hpp"
#include "shaderManager.hpp"
#include <vector>

BasePass::BasePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: m_device(device)
	, m_context(context)
	, m_shaderManager(std::make_unique<ShaderManager>(device))
{
	m_context.As(&m_annotation);
}

ComPtr<ID3D11RasterizerState> BasePass::createRSState(const RasterizerPreset preset) const
{
	switch (preset)
	{
	case RasterizerPreset::Default:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false);
	case RasterizerPreset::NoCullNoClip:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_SOLID, false, false);
	case RasterizerPreset::BackCull:
		return createRSState(D3D11_CULL_BACK, D3D11_FILL_SOLID, false, false);
	case RasterizerPreset::Wireframe:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_WIREFRAME, true, true);
	case RasterizerPreset::FrontCull:
		return createRSState(D3D11_CULL_FRONT, D3D11_FILL_SOLID, false, false);
	default:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false);
	}
}

ComPtr<ID3D11RasterizerState> BasePass::createRSState(const D3D11_CULL_MODE cullMode,
                                                      const D3D11_FILL_MODE fillMode,
                                                      const bool depthClipEnable,
                                                      const bool antialiasedLineEnable) const
{
	D3D11_RASTERIZER_DESC desc = {};
	desc.CullMode = cullMode;
	desc.FillMode = fillMode;
	desc.DepthClipEnable = depthClipEnable;
	desc.AntialiasedLineEnable = antialiasedLineEnable;
	desc.FrontCounterClockwise = false;
	desc.MultisampleEnable = false;
	desc.ScissorEnable = false;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0.0f;
	desc.SlopeScaledDepthBias = 0.0f;

	ComPtr<ID3D11RasterizerState> state;
	m_device->CreateRasterizerState(&desc, &state);
	return state;
}

ComPtr<ID3D11DepthStencilState> BasePass::createDSState(DepthStencilPreset preset)
{
	switch (preset)
	{
	case DepthStencilPreset::WriteDepth:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS);
	case DepthStencilPreset::ReadOnlyEqual:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_EQUAL);
	case DepthStencilPreset::ReadOnlyLessEqual:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_LESS_EQUAL);
	case DepthStencilPreset::Disabled:
		return createDSState(false, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS);
	default:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS);
	}
}

ComPtr<ID3D11DepthStencilState> BasePass::createDSState(
	const bool depthEnable,
	const D3D11_DEPTH_WRITE_MASK writeMask,
	const D3D11_COMPARISON_FUNC depthFunc) const
{
	D3D11_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = depthEnable;
	desc.DepthWriteMask = writeMask;
	desc.DepthFunc = depthFunc;
	desc.StencilEnable = false;
	desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	ComPtr<ID3D11DepthStencilState> state;
	m_device->CreateDepthStencilState(&desc, &state);
	return state;
}

ComPtr<ID3D11SamplerState> BasePass::createSamplerState(const SamplerPreset preset)
{
	switch (preset)
	{
	case SamplerPreset::LinearWrap:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	case SamplerPreset::LinearClamp:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	case SamplerPreset::AnisotropicWrap:
		return createSamplerState(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
	case SamplerPreset::PointClamp:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	default:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	}
}

ComPtr<ID3D11SamplerState> BasePass::createSamplerState(
	const D3D11_FILTER filter,
	const D3D11_TEXTURE_ADDRESS_MODE addressMode)
{
	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = filter;
	desc.AddressU = addressMode;
	desc.AddressV = addressMode;
	desc.AddressW = addressMode;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	desc.MaxAnisotropy = (filter == D3D11_FILTER_ANISOTROPIC) ? 16 : 1;

	ComPtr<ID3D11SamplerState> state;
	m_device->CreateSamplerState(&desc, &state);
	return state;
}

void BasePass::setViewport(const UINT width, const UINT height) const
{
	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	m_context->RSSetViewports(1, &viewport);
}

void BasePass::unbindRenderTargets(const UINT count) const
{
	const std::vector<ID3D11RenderTargetView*> nullRTVs(count, nullptr);
	m_context->OMSetRenderTargets(count, nullRTVs.data(), nullptr);
}

void BasePass::unbindShaderResources(const UINT startSlot, const UINT count)
{
	const std::vector<ID3D11ShaderResourceView*> nullSRVs(count, nullptr);
	m_context->PSSetShaderResources(startSlot, count, nullSRVs.data());
	m_context->CSSetShaderResources(startSlot, count, nullSRVs.data());
}

void BasePass::unbindComputeUAVs(const UINT startSlot, const UINT count) const
{
	if (count == 0)
		return;
	const std::vector<ID3D11UnorderedAccessView*> nullUAVs(count, nullptr);
	m_context->CSSetUnorderedAccessViews(startSlot, count, nullUAVs.data(), nullptr);
}

void BasePass::beginDebugEvent(const wchar_t* name) const
{
	if (m_annotation)
	{
		m_annotation->BeginEvent(name);
	}
}

void BasePass::endDebugEvent() const
{
	if (m_annotation)
	{
		m_annotation->EndEvent();
	}
}
