#pragma once

#include <memory>

#include <d3d11_4.h>
#include <wrl.h>

#include "basePass.hpp"

class RTVCollector;

using namespace Microsoft::WRL;


class FSQuad : public BasePass
{
public:
	FSQuad(ComPtr<ID3D11Device> _device, ComPtr<ID3D11DeviceContext> _context);
	void draw(ComPtr<ID3D11ShaderResourceView> srv);
	void createOrResize();
	const ComPtr<ID3D11ShaderResourceView>& getSRV() const;

private:
	std::unique_ptr<RTVCollector> m_rtvCollector;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;
	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11Texture2D> m_texture;
};
