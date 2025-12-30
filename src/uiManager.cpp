#include "uiManager.hpp"

#include <iostream>
#include <glm/gtc/type_ptr.hpp>

#define IM_VEC2_CLASS_EXTRA
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "ImGuizmo.h"

#include "appConfig.hpp"
#include "inputEventsHandler.hpp"
#include "debugPassMacros.hpp"
#include "passes/GBuffer.hpp"

#include "scene.hpp"
#include "light.hpp"
#include "primitive.hpp"
#include "camera.hpp"

#include "commands/nodeCommand.hpp"
#include "commands/nodeSnapshot.hpp"
#include "commands/scopedTransaction.hpp"
#include "commands/commandManager.hpp"

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
static bool gUseSnap(false);
static glm::ivec3 gSnapTranslate = { 1, 1, 1 };
static glm::ivec3 gSnapRotate = { 15, 15, 15 };
static glm::ivec3 gSnapScale = { 1, 1, 1 };

UIManager::UIManager(const ComPtr<ID3D11Device>& device,
	const ComPtr<ID3D11DeviceContext>& deviceContext,
	const HWND& hwnd) :
	m_device(device),
	m_hwnd(hwnd),
	m_context(deviceContext),
	m_commandManager(std::make_unique<CommandManager>())
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

void UIManager::draw(const ComPtr<ID3D11ShaderResourceView>& srv, const GBuffer& gbuffer, Scene* scene, const glm::mat4& view, const glm::mat4& projection)
{
	m_view = view;
	m_projection = projection;
	DEBUG_PASS_START(L"UIManager Draw");
	m_scene = scene;
	// Always check window validity before starting a new frame
	if (!IsWindow(m_hwnd))
	{
		DEBUG_PASS_END();
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();


	showMainMenuBar();
	showInvisibleDockWindow();
	showSceneSettings();
	showViewport(srv);
	drawSceneGraph();
	showProperties();
	showMaterialBrowser();
	processInputEvents();
	processNodeDeletion();
	processNodeDuplication();
	processUndoRedo();

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

void UIManager::showMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "Ctrl+N")) { /* TODO: New scene */ }
			if (ImGui::MenuItem("Open...", "Ctrl+O")) { /* TODO: Open file dialog */ }
			if (ImGui::MenuItem("Save", "Ctrl+S")) { /* TODO: Save scene */ }
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) { /* TODO: Save as dialog */ }
			ImGui::Separator();
			if (ImGui::MenuItem("Import Model...", "Ctrl+I")) { /* TODO: Import model */ }
			if (ImGui::MenuItem("Export...", "Ctrl+E")) { /* TODO: Export */ }
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "Alt+F4")) { PostQuitMessage(0); }
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) { /* TODO: Undo */ }
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) { /* TODO: Redo */ }
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X", false, false)) { /* TODO: Cut */ }
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, false)) { /* TODO: Copy */ }
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) { /* TODO: Paste */ }
			if (ImGui::MenuItem("Delete", "Del")) { /* TODO: Delete selected */ }
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "Ctrl+A")) { /* TODO: Select all */ }
			if (ImGui::MenuItem("Deselect All", "Ctrl+D")) { m_scene->clearSelectedNodes(); }
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Viewport", nullptr, true)) { /* Toggle viewport */ }
			if (ImGui::MenuItem("Scene Graph", nullptr, true)) { /* Toggle scene graph */ }
			if (ImGui::MenuItem("Properties", nullptr, true)) { /* Toggle properties */ }
			if (ImGui::MenuItem("Material Browser", nullptr, true)) { /* Toggle material browser */ }
			if (ImGui::MenuItem("GBuffer", nullptr, true)) { /* Toggle GBuffer view */ }
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout")) { /* TODO: Reset docking layout */ }
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::BeginMenu("Mesh"))
			{
				if (ImGui::MenuItem("Cube")) { /* TODO: Add cube */ }
				if (ImGui::MenuItem("Sphere")) { /* TODO: Add sphere */ }
				if (ImGui::MenuItem("Plane")) { /* TODO: Add plane */ }
				if (ImGui::MenuItem("Cylinder")) { /* TODO: Add cylinder */ }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Light"))
			{
				if (ImGui::MenuItem("Point Light")) { /* TODO: Add point light */ }
				if (ImGui::MenuItem("Directional Light")) { /* TODO: Add directional light */ }
				if (ImGui::MenuItem("Spot Light")) { /* TODO: Add spot light */ }
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Empty Node")) { /* TODO: Add empty node */ }
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render"))
		{
			if (ImGui::MenuItem("Render Image")) { /* TODO: Render to file */ }
			ImGui::Separator();
			if (ImGui::BeginMenu("Debug Views"))
			{
				if (ImGui::MenuItem("Albedo")) { /* TODO: Show albedo only */ }
				if (ImGui::MenuItem("Normals")) { /* TODO: Show normals */ }
				if (ImGui::MenuItem("Metallic/Roughness")) { /* TODO: Show metallic/roughness */ }
				if (ImGui::MenuItem("Depth")) { /* TODO: Show depth */ }
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About")) { /* TODO: Show about dialog */ }
			if (ImGui::MenuItem("Documentation")) { /* TODO: Open docs */ }
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void UIManager::showInvisibleDockWindow()
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("InvisibleDockSpaceWindow", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}

void UIManager::showMaterialBrowser()
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
	ImGui::Begin("Material Browser", nullptr, windowFlags);
	std::vector<std::string> materialNames = m_scene->getMaterialNames();
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
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("Viewport", nullptr, windowFlags);
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

	// Set up ImGuizmo to draw in this viewport
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImGuizmo::SetRect(windowPos.x, windowPos.y, size.x, size.y);

	ImGui::Image(tex, size, uv0, uv1);
	// Snap toggle button in top right corner of viewport
	// Gizmo controls overlay
	ImGui::SetCursorPos(ImVec2(5.0f, 5.0f));
	ImGui::BeginChild("GizmoControls", ImVec2(200.0f, 150.0f), true, ImGuiWindowFlags_NoScrollbar);
	{
		if (ImGui::Button(gUseSnap ? "Snap: On" : "Snap: Off", ImVec2(-1.0f, 20.0f)))
		{
			gUseSnap = !gUseSnap;
		}

		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;

		ImGui::PushItemWidth(-1.0f);
		ImGui::Text("[T]");
		ImGui::SameLine();
		ImGui::SliderInt("##SnapTranslate", &gSnapTranslate[0], 0, 100);
		ImGui::Text("[R]");
		ImGui::SameLine();;
		ImGui::SliderInt("##SnapRotate", &gSnapRotate[0], 1, 180);
		ImGui::Text("[S]");
		ImGui::SameLine();
		ImGui::SliderInt("##SnapScale", &gSnapScale[0], 0, 100);
		ImGui::PopItemWidth();
	}
	ImGui::EndChild();

	processGizmo();

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

void UIManager::processGizmo()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode == nullptr)
		return;
	ImGuizmo::SetDrawlist();
	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_G))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_S)) // r Key
			mCurrentGizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_M))
			gUseSnap = !gUseSnap;
	}

	TScopedTransaction<Snapshot::SceneNodeTransform> nodeTransaction{ m_commandManager.get(), m_scene, activeNode };

	glm::mat4 worldMatrix = activeNode->getWorldMatrix();
	float* matrix = glm::value_ptr(worldMatrix);
	float* viewMatrix = const_cast<float*>(glm::value_ptr(m_view));
	float* projectionMatrix = const_cast<float*>(glm::value_ptr(m_projection));
	glm::vec3 currentSnapValue;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		currentSnapValue = gSnapTranslate;
		break;
	case ImGuizmo::ROTATE:
		currentSnapValue = glm::vec3(gSnapRotate);
		break;
	case ImGuizmo::SCALE:
		currentSnapValue = gSnapScale;
		break;
	default:
		currentSnapValue = glm::vec3(0.0f);
		break;
	}
	ImGuizmo::Manipulate(viewMatrix, projectionMatrix, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, gUseSnap ? &currentSnapValue[0] : NULL);
	if (ImGuizmo::IsUsing())
	{
		m_isMouseInViewport = false;

		// Convert the modified world matrix back to local space
		glm::mat4 newWorldMatrix = worldMatrix; // matrix was modified in-place
		glm::mat4 localMatrix;
		if (activeNode->parent && !dynamic_cast<Scene*>(activeNode->parent))
		{
			// Has a non-Scene parent: convert world -> local
			glm::mat4 parentWorldInverse = glm::inverse(activeNode->parent->getWorldMatrix());
			localMatrix = parentWorldInverse * newWorldMatrix;
		}
		else
		{
			// No parent or parent is Scene: world == local
			localMatrix = newWorldMatrix;
		}
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), matrixTranslation, matrixRotation, matrixScale);
		activeNode->transform.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
		activeNode->transform.rotation = glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
		activeNode->transform.scale = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
		if (gUseSnap && mCurrentGizmoOperation == ImGuizmo::TRANSLATE)
		{
			ImGuizmo::DrawGrid(viewMatrix, projectionMatrix, glm::value_ptr(newWorldMatrix), 100.0f);
		}
		if (dynamic_cast<Primitive*>(activeNode))
		{
			m_scene->markSceneBVHDirty();
		}
		if (dynamic_cast<Light*>(activeNode))
		{
			m_scene->setLightsDirty();
		}
	}
}

void UIManager::processNodeDuplication()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode && ImGui::IsKeyPressed(ImGuiKey_D, false) && InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		auto createSceneNode = std::make_unique<Command::DuplicateSceneNode>(m_scene, activeNode);
		m_commandManager->commitCommand(std::move(createSceneNode));
	}
}

void UIManager::processNodeDeletion()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		bool isPrimitive = dynamic_cast<Primitive*>(activeNode) != nullptr;
		bool isLight = dynamic_cast<Light*>(activeNode) != nullptr;
		auto removeSceneNode = std::make_unique<Command::RemoveSceneNode>(m_scene, activeNode);
		m_commandManager->commitCommand(std::move(removeSceneNode));
		if (isPrimitive)
		{
			m_scene->markSceneBVHDirty();
		}
		if (isLight)
		{
			m_scene->setLightsDirty();
		}
	}
}

void UIManager::processUndoRedo()
{
	// We put a merge fence when mouse is released
	bool isMouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right);
	if (isMouseReleased)
	{
		m_commandManager->setMergeFence();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		if (ImGui::IsKeyDown(ImGuiMod_Shift))
		{
			if (m_commandManager->hasRedoCommands())
			{
				m_commandManager->redo();
			}
		}
		else
		{
			if (m_commandManager->hasUndoCommands())
			{
				m_commandManager->undo();
			}
		}
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		if (m_commandManager->hasRedoCommands())
		{
			m_commandManager->redo();
		}
	}
}

void UIManager::drawSceneGraph()
{
	ImGui::Begin("Scene Graph");
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	if (ImGui::BeginTable("SceneGraph", 1, tableFlags, availableSize))
	{
		ImGui::TableSetupColumn(m_scene->name.c_str(), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		drawNode(m_scene);
		ImGui::EndTable();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
			{
				SceneNode* draggedNode = *(SceneNode**)payload->Data;
				if (draggedNode && draggedNode->parent)
				{
					std::unique_ptr<SceneNode> nodePtr = draggedNode->parent->removeChild(draggedNode);
					m_scene->addChild(std::move(nodePtr));
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
			drawNode(child.get());
		}
		return;
	}

	const bool isFolder = (node->children.size() > 0);
	const char* icon = getNodeIcon(node);
	std::string label = std::string(icon) + " " + node->name;
	if (isFolder)
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool isSelected = m_scene->isNodeSelected(node);
		if (isSelected)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		bool open = ImGui::TreeNodeEx(label.c_str(), nodeFlags);
		handleNodeSelection(node);
		handleNodeDragDrop(node);

		if (open)
		{
			for (auto& child : node->children)
			{
				drawNode(child.get());
			}
			ImGui::TreePop();
		}

		ImGui::TableNextColumn();
	}
	else
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		bool isSelected = m_scene->isNodeSelected(node);
		if (isSelected)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}
		ImGui::TreeNodeEx(label.c_str(), nodeFlags);

		handleNodeDragDrop(node);
		handleNodeSelection(node);
		ImGui::TableNextColumn();
	}
}

const char* UIManager::getNodeIcon(SceneNode* node)
{
	if (dynamic_cast<Light*>(node))
	{
		return "[L]"; // Light
	}
	else if (dynamic_cast<Primitive*>(node))
	{
		return "[P]"; // Primitive
	}
	else if (!node->children.empty())
	{
		return "[F]"; // Folder
	}
	else if (dynamic_cast<Camera*>(node))
	{
		return "[C]"; // Camera
	}

	return "[D]"; // Default document icon
}

void UIManager::handleNodeSelection(SceneNode* node)
{
	if (ImGui::IsItemClicked())
	{
		bool addToSelection = InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT);
		m_scene->setActiveNode(node, addToSelection);
	}
}

void UIManager::handleNodeDragDrop(SceneNode* targetNode)
{
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_NODE", &targetNode, sizeof(SceneNode*));
		ImGui::Text("%s", targetNode->name.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
		{
			SceneNode* draggedNode = *reinterpret_cast<SceneNode**>(payload->Data);
			if (draggedNode && draggedNode != targetNode && draggedNode->parent)
			{
				std::unique_ptr<SceneNode> nodePtr = draggedNode->parent->removeChild(draggedNode);
				if (nodePtr)
				{
					targetNode->addChild(std::move(nodePtr));
				}
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void UIManager::showProperties()
{
	ImGui::Begin("Properties");
	if (m_scene->getActiveNode())
	{
		if (dynamic_cast<Primitive*>(m_scene->getActiveNode()))
		{
			showPrimitiveProperties(dynamic_cast<Primitive*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<Light*>(m_scene->getActiveNode()))
		{
			showLightProperties(dynamic_cast<Light*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<Camera*>(m_scene->getActiveNode()))
		{
			showCameraProperties(dynamic_cast<Camera*>(m_scene->getActiveNode()));
		}
		ImGui::Text("Selected Node: %s", m_scene->getActiveNode()->name.c_str());
	}
	ImGui::End();
}

void UIManager::showPrimitiveProperties(Primitive* prim)
{
	{
		TScopedTransaction<Snapshot::SceneNodeTransform> nodeTransaction{ m_commandManager.get(), m_scene, prim };

		ImGui::Text("Type: Primitive");
		ImGui::Text("Name: %s", prim->name.c_str());
		ImGui::DragFloat3("Position", &prim->transform.position[0], 0.1f);
		ImGui::DragFloat3("Rotation", &prim->transform.rotation[0], 0.1f);
		ImGui::DragFloat3("Scale", &prim->transform.scale[0], 0.1f);
	}

	{
		const auto& materialNames = m_scene->getMaterialNames();
		int currentMaterialIndex = -1;
		// Find current material index
		if (currentMaterialIndex < 0 && prim->material)
		{
			int index = 0;
			for (const auto& name : materialNames)
			{
				if (m_scene->getMaterial(name) == prim->material)
				{
					currentMaterialIndex = index;
					break;
				}
				index++;
			}
		}

		// Save previous material index
		int previousMaterialIndex = currentMaterialIndex;
		if (ImGui::BeginCombo("Material", currentMaterialIndex >= 0 && currentMaterialIndex < materialNames.size() ? materialNames[currentMaterialIndex].c_str() : "Select Material"))
		{
			for (int n = 0; n < static_cast<int>(materialNames.size()); n++)
			{
				bool isSelected = (currentMaterialIndex == n);
				if (ImGui::Selectable(materialNames[n].c_str(), isSelected))
				{
					currentMaterialIndex = n;
					if (currentMaterialIndex != previousMaterialIndex)
					{
						TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{ m_commandManager.get(), m_scene, prim };
						std::shared_ptr<Material> selectedMaterial = m_scene->getMaterial(materialNames[n]);
						prim->material = selectedMaterial;
					}
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
}

void UIManager::showMaterialProperties(std::shared_ptr<Material> material)
{
	return;
}

void UIManager::showLightProperties(Light* light)
{
	TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{ m_commandManager.get(), m_scene, light };

	ImGui::Text("Name: %s", light->name.c_str());

	ImGui::Combo(
		"Type",
		reinterpret_cast<int*>(&light->type),
		"Point Light\0Directional Light\0Spot Light\0");
	ImGui::DragFloat3("Position", &light->transform.position[0], 0.1f);
	ImGui::DragFloat3("Rotation", &light->transform.rotation[0], 0.1f);
	ImGui::ColorEdit3("Color", &light->color[0]);
	ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 100.0f);
}

void UIManager::showCameraProperties(Camera* camera)
{
	TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{ m_commandManager.get(), m_scene, camera };

	ImGui::Text("Name: %s", camera->name.c_str());
	ImGui::DragFloat3("Position", &camera->orbitPivot[0], 0.1f);
	ImGui::DragFloat3("Rotation", &camera->transform.rotation[0], 0.1f);
	ImGui::DragFloat("Fov", &camera->fov, 0.1f, 1.0f, 120.0f);
}

void UIManager::showSceneSettings()
{
	static float f = 0.0f;
	static int counter = 0;

	ImGui::Begin("SceneSettings");
	ImGui::DragFloat("IBL Intensity", &AppConfig::getIBLIntensity(), 1.0f, 0.0f, 100.0f);
	ImGui::DragFloat("IBL Rotation", &AppConfig::getIBLRotation());
	ImGui::DragFloat("Environment Map Intensity", &AppConfig::getBackgroundIntensity(), 1.0f, 0.0f, 1.0f);
	ImGui::Separator();

	ImGui::Checkbox("Blur Environment Map", &AppConfig::getIsBlurred());
	if (AppConfig::getIsBlurred())
	{
		ImGui::SliderFloat("Blur Amount", &AppConfig::getBlurAmount(), 0.0f, 5.0f);
	}
	ImGui::Checkbox("Regenerate Prefiltered Map", &AppConfig::getRegeneratePrefilteredMap());
	ImGui::Separator();

	// Debug BVH visualization
	ImGui::TextWrapped("Debug Visualization");
	ImGui::Checkbox("Show BVH", &AppConfig::getShowBVH());
	if (AppConfig::getShowBVH())
	{
		ImGui::Checkbox("Show Primitive BVH (triangles)", &AppConfig::getShowPrimitiveBVH());
		ImGui::SliderInt("BVH Max Depth", &AppConfig::getBVHMaxDepth(), -1, 20, AppConfig::getBVHMaxDepth() < 0 ? "All" : "%d");
	}

	ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)",
		1000.0f / m_io->Framerate, m_io->Framerate);
	ImGui::End();

}