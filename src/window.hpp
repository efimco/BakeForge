#pragma once

// windows.h disahle minmax
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif

#include <windows.h>
#include <d3d11.h>

class Window
{
public:
	Window(HINSTANCE hInstance);
	~Window() = default;

	const HWND& getHandle() const;


private:
	HWND m_hWindow;
	WNDCLASS m_windClass;
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};