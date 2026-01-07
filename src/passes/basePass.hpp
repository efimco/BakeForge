#pragma once
#include <d3d11_4.h>
#include <memory>
#include <wrl.h>

class ShaderManager;

enum class RasterizerPreset
{
	Default
	, // CullNone, Solid, DepthClip=true
	NoCullNoClip
	, // CullNone, Solid, DepthClip=false (GBuffer, WorldSpaceUI)
	BackCull
	, // CullBack, Solid (CubeMap)
	FrontCull
	,         // CullFront, Solid (CubeMap)
	Wireframe // CullNone, Wireframe, AntialiasedLine (DebugBVH)
};

enum class DepthStencilPreset
{
	WriteDepth
	, // DepthWrite=All, Func=Less (ZPrePass)
	ReadOnlyEqual
	, // DepthWrite=Zero, Func=Equal (GBuffer, CubeMap)
	ReadOnlyLessEqual
	,        // DepthWrite=Zero, Func=LessEqual (DebugBVH)
	Disabled // DepthEnable=false
};

enum class SamplerPreset
{
	LinearWrap
	, // Linear filter, Wrap
	LinearClamp
	, // Linear filter, Clamp
	AnisotropicWrap
	,          // Anisotropic filter, Wrap
	PointClamp // Point filter, Clamp
};

using namespace Microsoft::WRL;

class BasePass
{
public:
	explicit BasePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	virtual ~BasePass() = default;

	BasePass(const BasePass& other) = delete;
	BasePass& operator=(const BasePass& other) = delete;

protected:
	ComPtr<ID3D11RasterizerState> createRSState(RasterizerPreset preset) const;
	ComPtr<ID3D11RasterizerState> createRSState(D3D11_CULL_MODE cullMode
	                                            , D3D11_FILL_MODE fillMode
	                                            , bool depthClipEnable = true
	                                            , bool antialiasedLineEnable = false) const;

	ComPtr<ID3D11DepthStencilState> createDSState(DepthStencilPreset preset);
	ComPtr<ID3D11DepthStencilState> createDSState(bool depthEnable
	                                              , D3D11_DEPTH_WRITE_MASK writeMask
	                                              , D3D11_COMPARISON_FUNC depthFunc) const;

	ComPtr<ID3D11SamplerState> createSamplerState(SamplerPreset preset);
	ComPtr<ID3D11SamplerState> createSamplerState(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode);

	void setViewport(UINT width, UINT height) const;
	void unbindRenderTargets(UINT count) const;
	void unbindShaderResources(UINT startSlot, UINT count);
	void unbindComputeUAVs(UINT startSlot, UINT count) const;

	// Debug annotation (for graphics debuggers like RenderDoc/PIX)
	void beginDebugEvent(const wchar_t* name) const;
	void endDebugEvent() const;


	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	std::unique_ptr<ShaderManager> m_shaderManager;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11BlendState> m_blendState;

private:
	ComPtr<ID3DUserDefinedAnnotation> m_annotation;
};
