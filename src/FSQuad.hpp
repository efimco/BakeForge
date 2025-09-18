#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "shaderManager.hpp"

using namespace Microsoft::WRL;


inline static const D3D11_INPUT_ELEMENT_DESC FSQuadInputLayoutDesc[] =
{
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

class FSQuad
{
public:
	FSQuad(const ComPtr<ID3D11Device>& _device, const ComPtr<ID3D11DeviceContext>& _context);
	void draw(const ComPtr<ID3D11ShaderResourceView>& srv);
	void createOrResize();
	const ComPtr<ID3D11ShaderResourceView>& getSRV() const;

private:

	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;
	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11SamplerState> m_samplerState;
	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_context;

	ShaderManager* m_shaderManager;

};