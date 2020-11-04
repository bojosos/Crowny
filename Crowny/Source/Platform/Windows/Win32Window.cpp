#if 0
#include "cwpch.h"

#include "Platform/Windows/Win32Window.h"
#include <Windows.h>
#include <mciapi.h>

namespace Crowny
{
	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	Win32Window::Win32Window(const WindowProperties& props)
	{
		Init(props);
	}

	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
	{
		WNDCLASS wc = { };
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		//wc.lpszClassName = props.Title.c_str();
		RegisterClass(&wc);

		HWND hwnd = CreateWindowEx(0, "Test window class", props.Title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, props.Width, props.Height, nullptr, nullptr, hInstance, nullptr);

		if (hwnd == nullptr)
			return;

		ShowWindow(hwnd, nCmdShow);
	}

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_PAINT:
			{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			EndPaint(hwnd, &ps);
			}
		}
		return 0;

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

}
#endif