#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "imgui.h"
#include "GBuffer.hpp"
using namespace Microsoft::WRL;

class UIManager
{
public:
	UIManager(const ComPtr<ID3D11Device>& device,
		const ComPtr<ID3D11DeviceContext>& deviceContext,
		const HWND& hwnd);
	~UIManager();

	void draw(const ComPtr<ID3D11ShaderResourceView>& srv, const GBuffer& gbuffer, SceneNode* scene);
	uint32_t* getMousePos();

private:
	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_context;
	const HWND& m_hwnd;
	ImGuiIO* m_io;
	uint32_t m_mousePos[2];
	bool m_isMouseInViewport;
	void simpleWindow();
	void showViewport(const ComPtr<ID3D11ShaderResourceView>& srv);
	void showInvisibleDockWindow();
	void showGBufferImage(const GBuffer& gbuffer);
	void processInputEvents();
	void drawSceneGraph(SceneNode* scene);
	void drawNode(SceneNode* node);

};
