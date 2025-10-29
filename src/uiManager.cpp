#include "uiManager.hpp"
#define IM_VEC2_CLASS_EXTRA
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "appConfig.hpp"
#include <iostream>
#include "inputEventsHandler.hpp"
#include "sceneManager.hpp"
#include "debugPassMacros.hpp"
#include "scene.hpp"

const float TEXT_BASE_WIDTH = 1;

UIManager::UIManager(const ComPtr<ID3D11Device>& device,
	const ComPtr<ID3D11DeviceContext>& deviceContext,
	const HWND& hwnd) :
	m_device(device),
	m_hwnd(hwnd),
	m_context(deviceContext)
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
	ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

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

void UIManager::draw(const ComPtr<ID3D11ShaderResourceView>& srv, const GBuffer& gbuffer, Scene* scene)
{
	DEBUG_PASS_START(L"UIManager Draw");
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
	drawSceneGraph(scene);
	showProperties();
	showMaterialBrowser();
	processInputEvents();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
	DEBUG_PASS_END();
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

void UIManager::showMaterialBrowser()
{
	ImGui::Begin("Material Browser");
	std::vector<std::string> materialNames = SceneManager::getMaterialNames();
	ImVec2 contentRegion = ImGui::GetContentRegionAvail();
	float maxItemSize = 100.0f;
	int itemsPerRow = int(floor(contentRegion.x / (maxItemSize + ImGui::GetStyle().ItemSpacing.x)));
	float itemSize = (contentRegion.x - (itemsPerRow)*ImGui::GetStyle().ItemSpacing.x * 2) / itemsPerRow;
	for (int i = 0; i < materialNames.size(); i++)
	{
		if (ImGui::ImageButton(materialNames[i].c_str(), nullptr, ImVec2(itemSize, itemSize)))
		{
			// Material selected
		}
		if (itemsPerRow > 0 && (i + 1) % itemsPerRow != 0 && i < materialNames.size() - 1)
		{
			ImGui::SameLine();
		}
	}
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
	size.x = static_cast<int>(size.x) % 2 == 0 ? size.x : size.x - 1;
	size.y = static_cast<int>(size.y) % 2 == 0 ? size.y : size.y - 1;

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
	InputEvents::setMouseClicked(MouseButtons::LEFT_BUTTON, m_io->MouseClicked[0]);
	InputEvents::setMouseClicked(MouseButtons::MIDDLE_BUTTON, m_io->MouseClicked[2]);
	InputEvents::setMouseClicked(MouseButtons::RIGHT_BUTTON, m_io->MouseClicked[1]);

	InputEvents::setMouseDown(MouseButtons::LEFT_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Left));
	InputEvents::setMouseDown(MouseButtons::MIDDLE_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Middle));
	InputEvents::setMouseDown(MouseButtons::RIGHT_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Right));

	InputEvents::setMouseInViewport(m_isMouseInViewport);

	InputEvents::setMouseDelta(m_io->MouseDelta.x, m_io->MouseDelta.y);
	InputEvents::setMouseWheel(m_io->MouseWheel);

	InputEvents::setKeyDown(KeyButtons::KEY_W, ImGui::IsKeyDown(ImGuiKey_W));
	InputEvents::setKeyDown(KeyButtons::KEY_A, ImGui::IsKeyDown(ImGuiKey_A));
	InputEvents::setKeyDown(KeyButtons::KEY_S, ImGui::IsKeyDown(ImGuiKey_S));
	InputEvents::setKeyDown(KeyButtons::KEY_D, ImGui::IsKeyDown(ImGuiKey_D));
	InputEvents::setKeyDown(KeyButtons::KEY_Q, ImGui::IsKeyDown(ImGuiKey_Q));
	InputEvents::setKeyDown(KeyButtons::KEY_E, ImGui::IsKeyDown(ImGuiKey_E));
	InputEvents::setKeyDown(KeyButtons::KEY_LSHIFT, ImGui::IsKeyDown(ImGuiKey_LeftShift));

}

void UIManager::drawSceneGraph(Scene* scene)
{
	ImGui::Begin("Scene Graph");
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	if (ImGui::BeginTable("SceneGraph", 1, tableFlags, availableSize))
	{
		ImGui::TableSetupColumn(scene->name.c_str(), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		drawNode(scene);
		ImGui::EndTable();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
			{
				SceneNode* draggedNode = *(SceneNode**)payload->Data;
				if (draggedNode && draggedNode->parent)
				{
					draggedNode->parent->removeChild(draggedNode);
					scene->addChild(draggedNode);
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::End();
}

void UIManager::drawNode(SceneNode* node)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool isScene = dynamic_cast<Scene*>(node);
	if (isScene)
	{
		for (auto& child : node->children)
		{
			drawNode(child);
		}
		return;
	}

	const bool isFolder = (node->children.size() > 0);
	if (isFolder)
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool isSelected = SceneManager::isNodeSelected(node);
		if (isSelected)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}
		bool open = ImGui::TreeNodeEx(node->name.c_str(), nodeFlags);
		handleNodeSelection(node);

		handleNodeDragDrop(node);
		if (open)
		{
			for (auto& child : node->children)
			{
				drawNode(child);
			}
			ImGui::TreePop();
		}

		ImGui::TableNextColumn();
	}
	else
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		bool isSelected = SceneManager::isNodeSelected(node);
		if (isSelected)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}
		ImGui::TreeNodeEx(node->name.c_str(), nodeFlags);

		handleNodeDragDrop(node);
		handleNodeSelection(node);
		ImGui::TableNextColumn();
	}
}

void UIManager::handleNodeSelection(SceneNode* node)
{
	if (ImGui::IsItemClicked())
	{
		bool addToSelection = InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT);
		SceneManager::setSelectedNode(node, addToSelection);
		auto primitive = dynamic_cast<Primitive*>(node);
		if (primitive && !SceneManager::isPrimitiveSelected(primitive))
		{
			SceneManager::selectPrimitive(primitive, addToSelection);
		}
	}
}

void UIManager::handleNodeDragDrop(SceneNode* node)
{
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_NODE", &node, sizeof(SceneNode*));
		ImGui::Text("%s", node->name.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
		{
			SceneNode* draggedNode = *(SceneNode**)payload->Data;
			if (draggedNode && draggedNode != node && draggedNode->parent)
			{
				draggedNode->parent->removeChild(draggedNode);
				node->addChild(draggedNode);
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void UIManager::showProperties()
{
	ImGui::Begin("Properties");
	if (SceneManager::getSelectedNode())
	{
		if (dynamic_cast<Primitive*>(SceneManager::getSelectedNode()))
		{
			showPrimitiveProperties(dynamic_cast<Primitive*>(SceneManager::getSelectedNode()));
		}
		else if (dynamic_cast<Light*>(SceneManager::getSelectedNode()))
		{
			showLightProperties(dynamic_cast<Light*>(SceneManager::getSelectedNode()));
		}
		ImGui::Text("Selected Node: %s", SceneManager::getSelectedNode()->name.c_str());
	}
	ImGui::End();
}

void UIManager::showPrimitiveProperties(Primitive* prim)
{
	ImGui::Text("Type: Primitive");
	ImGui::Text("Name: %s", prim->name.c_str());
	if (ImGui::DragFloat3("Position", &prim->transform.position[0], 0.1f))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::DragFloat3("Rotation", &prim->transform.rotation[0], 0.1f))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::DragFloat3("Scale", &prim->transform.scale[0], 0.1f))
	{
		SceneManager::setLightsDirty(true);
	}

	const auto& materialNames = SceneManager::getMaterialNames();
	int currentMaterialIndex = -1;
	// Find current material index
	if (currentMaterialIndex < 0 && prim->material)
	{
		int index = 0;
		for (const auto& name : materialNames)
		{
			if (SceneManager::getMaterial(name) == prim->material)
			{
				currentMaterialIndex = index;
				break;
			}
			index++;
		}
	}

	if (ImGui::BeginCombo("Material", currentMaterialIndex >= 0 && currentMaterialIndex < materialNames.size() ? materialNames[currentMaterialIndex].c_str() : "Select Material"))
	{
		for (int n = 0; n < static_cast<int>(materialNames.size()); n++)
		{
			bool isSelected = (currentMaterialIndex == n);
			if (ImGui::Selectable(materialNames[n].c_str(), isSelected))
			{
				currentMaterialIndex = n;
				std::shared_ptr<Material> selectedMaterial = SceneManager::getMaterial(materialNames[n]);
				prim->material = selectedMaterial;
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

void UIManager::showMaterialProperties(std::shared_ptr<Material> material)
{
	return;
}

void UIManager::showLightProperties(Light* light)
{
	ImGui::Text("Name: %s", light->name.c_str());

	if (ImGui::Combo("Type", reinterpret_cast<int*>(&light->type),
		"Point Light\0Directional Light\0Spot Light\0"))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::DragFloat3("Position", &light->transform.position[0], 0.1f))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::DragFloat3("Rotation", &light->transform.rotation[0], 0.1f))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::ColorEdit3("Color", &light->color[0]))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f))
	{
		SceneManager::setLightsDirty(true);
	}
	if (ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 100.0f))
	{
		SceneManager::setLightsDirty(true);
	}
}


void UIManager::simpleWindow()
{
	static float f = 0.0f;
	static int counter = 0;

	ImGui::Begin("Hello, world!");
	ImGui::TextWrapped("This is some useful text.");
	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);

	if (ImGui::Button("Button"))
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)",
		1000.0f / m_io->Framerate, m_io->Framerate);
	ImGui::End();

}