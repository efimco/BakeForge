#include "window.hpp"

#include "appConfig.hpp"
#include "imgui.h"

Window::Window(HINSTANCE hInstance)
	: m_hWindow(NULL)
{
	const auto CLASS_NAME = "Sample Window Class";
	const auto WINDOW_NAME = "TimeToDX";
	m_windClass = {};
	m_windClass.lpfnWndProc = Window::WindowProc;
	m_windClass.hInstance = hInstance;
	m_windClass.lpszClassName = CLASS_NAME;
	m_windClass.style = CS_HREDRAW | CS_VREDRAW;
	m_windClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	m_windClass.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));

	RegisterClass(&m_windClass);

	m_hWindow = CreateWindowEx(
		0,
		CLASS_NAME,
		WINDOW_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (m_hWindow != nullptr)
	{
		ShowWindow(m_hWindow, SW_SHOWDEFAULT);
	}
}

const HWND& Window::getHandle() const
{
	return m_hWindow;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
	case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
	case WM_SIZE:
		{
			const int width = LOWORD(lParam);
			const int height = HIWORD(lParam);
			if (wParam == SIZE_MINIMIZED)
			{
				AppConfig::setWindowMinimized(true);
			}
			else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
			{
				AppConfig::setWindowMinimized(false);
				AppConfig::setWindowHeight(height);
				AppConfig::setWindowWidth(width);
				AppConfig::setNeedsResize(true);
			}
			break;
		}

	case WM_PAINT:
		{
			PAINTSTRUCT paintStruct = {};
			BeginPaint(hwnd, &paintStruct);
			EndPaint(hwnd, &paintStruct);
			break;
		}
	case WM_CLOSE:
		{
			if (MessageBoxW(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
			{
				DestroyWindow(hwnd);
			}
			return 0;
		}
	default:;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
