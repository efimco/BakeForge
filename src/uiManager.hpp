#pragma once
#include <d3d11.h>
#include <wrl.h>

using namespace Microsoft::WRL;

class UIManager
{
public:
	UIManager(const ComPtr<ID3D11Device>& device,
		const ComPtr<ID3D11DeviceContext>& deviceContext,
		const HWND& hwnd);
	~UIManager();
	
	void beginDraw();
	void endDraw();

private:
	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_deviceContext;
	const HWND& m_hwnd;
};