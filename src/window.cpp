#include "window.hpp"
#include <iostream>
#include "appConfig.hpp"
#include "imgui.h"
#include <ShellScalingApi.h>

Window::Window(HINSTANCE hInstance) : m_hWindow(NULL)
{
	// Set DPI awareness to per-monitor DPI aware (version 2)
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	LPCSTR CLASS_NAME = "Sample Window Class";
	LPCSTR WINDOW_NAME = "TimeToDX";
	m_windClass = {};
	m_windClass.lpfnWndProc = Window::WindowProc;
	m_windClass.hInstance = hInstance;
	m_windClass.lpszClassName = CLASS_NAME;
	m_windClass.style = CS_HREDRAW | CS_VREDRAW;
	m_windClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	m_windClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	RegisterClass(&m_windClass);

	m_hWindow = CreateWindowEx(
		0,
		CLASS_NAME,
		WINDOW_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (m_hWindow != NULL)
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
		break;
	}
	case WM_SIZE:
	{
		int width = LOWORD(lParam) % 2 == 0 ? LOWORD(lParam) : LOWORD(lParam) + 1;
		int height = HIWORD(lParam) % 2 == 0 ? HIWORD(lParam) : HIWORD(lParam) + 1;
		AppConfig::setWindowHeight(height);
		AppConfig::setWindowWidth(width);
		AppConfig::setNeedsResize(true);
		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT paintStruct = {};
		HDC hdc = BeginPaint(hwnd, &paintStruct);
		EndPaint(hwnd, &paintStruct);
		break;
	}
	case WM_CLOSE:
	{
		if (MessageBoxW(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
		{
			DestroyWindow(hwnd);
		}
		// Else: User canceled. Do nothing.
		break;
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}