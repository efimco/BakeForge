#include "uiManager.hpp"
#define IM_VEC2_CLASS_EXTRA
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "appConfig.hpp"
#include <iostream>
#include "inputEventsHandler.hpp"

UIManager::UIManager(const ComPtr<ID3D11Device>& device,
	const ComPtr<ID3D11DeviceContext>& deviceContext,
	const HWND& hwnd) :
	m_device(device),
	m_hwnd(hwnd),
	m_deviceContext(deviceContext)
{
	ImGui_ImplWin32_EnableDpiAwareness();
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	m_io = &ImGui::GetIO(); (void*)m_io;
	m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	m_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	m_io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
	m_io->ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
	m_io->ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(m_device.Get(), m_deviceContext.Get());

	// сonfigure ImGui multi-viewport support to use modern flip model swap chains
	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		DXGI_SWAP_CHAIN_DESC flipDesc = {};
		flipDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		flipDesc.SampleDesc.Count = 1;
		flipDesc.SampleDesc.Quality = 0;
		flipDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		flipDesc.BufferCount = 2;
		flipDesc.Windowed = TRUE;
		flipDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		flipDesc.Flags = 0;

		extern void ImGui_ImplDX11_SetSwapChainDescs(const DXGI_SWAP_CHAIN_DESC * desc_templates, int desc_templates_count);
		ImGui_ImplDX11_SetSwapChainDescs(&flipDesc, 1);
	}
	m_isMouseInViewport = false;
}

UIManager::~UIManager()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void UIManager::draw(const ComPtr<ID3D11ShaderResourceView>& srv, const GBuffer& gbuffer)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	if (!IsWindow(m_hwnd))
	{
		return;
	}
	showInvisibleDockWindow();
	simpleWindow();
	showViewport(srv);
	showGBufferImage(gbuffer);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

uint32_t* UIManager::getMousePos()
{
	return m_mousePos;
}

void UIManager::showInvisibleDockWindow()
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("InvisibleDockSpaceWindow", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}


void UIManager::showViewport(const ComPtr<ID3D11ShaderResourceView>& srv)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Viewport");
	m_isMouseInViewport = ImGui::IsWindowHovered();


	ImVec2 uv0 = ImVec2(0, 0);
	ImVec2 uv1 = ImVec2(1, 1);
	static ImVec2 prevSize = ImGui::GetContentRegionAvail();
	ImVec2 size = ImGui::GetContentRegionAvail();

	AppConfig::setViewportWidth(static_cast<int>(size.x));
	AppConfig::setViewportHeight(static_cast<int>(size.y));

	if (size.x != prevSize.x || size.y != prevSize.y)
	{
		AppConfig::setNeedsResize(true);
		prevSize = size;
	}

	ImTextureRef tex = srv.Get();
	ImVec2 mousePos = ImVec2(m_io->MousePos.x - ImGui::GetWindowPos().x, m_io->MousePos.y - ImGui::GetWindowPos().y);
	m_mousePos[0] = static_cast<uint32_t>(mousePos.x);
	m_mousePos[1] = static_cast<uint32_t>(mousePos.y);
	ImGui::Image(tex, size, uv0, uv1);

	ImGui::End();
	ImGui::PopStyleVar(3);
}

void UIManager::showGBufferImage(const GBuffer& gbuffer)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("GBuffer");

	ImVec2 uv0 = ImVec2(0, 0);
	ImVec2 uv1 = ImVec2(1, 1);
	ImVec2 contentSize = ImGui::GetContentRegionAvail();
	float aspectRatio = static_cast<float>(AppConfig::getViewportWidth()) / static_cast<float>(AppConfig::getViewportHeight());

	ImVec2 size;
	if (contentSize.x / contentSize.y > aspectRatio)
	{
		// Content is wider than needed, fit by height
		size.y = contentSize.y;
		size.x = size.y * aspectRatio;
	}
	else
	{
		// Content is taller than needed, fit by width
		size.x = contentSize.x;
		size.y = size.x / aspectRatio;
	}
	ImTextureRef tex = gbuffer.getAlbedoSRV().Get();
	ImGui::Image(tex, size, uv0, uv1);

	ImGui::End();
	ImGui::PopStyleVar(3);
}

void UIManager::processInputEvents()
{
	InputEvents::setMouseInViewport(m_isMouseInViewport);
	InputEvents::setMouseClicked(m_io->MouseClicked);
}


void UIManager::simpleWindow()
{
	static float f = 0.0f;
	static int counter = 0;

	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

	ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

	if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / m_io->Framerate, m_io->Framerate);
	ImGui::End();

}