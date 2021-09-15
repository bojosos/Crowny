#if 0
#pragma once

#include "Crowny/Window/Window.h"

namespace Crowny
{

	class Win32Window : public Window
	{
	public:
		Win32Window(const WindowProperties& props);
		~Win32Window();

		virtual void OnUpdate() override;

		virtual uint32_t GetWidth() const override { return m_Data.Width; };
		virtual uint32_t GetHeight() const override { return m_Data.Height; }

		virtual void SetCursor(Cursor cursor) override;

		virtual void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; };
		virtual void* GetNativeWindow() const override { return m_Window; }

		virtual void SetVSync(bool enabled) override;
		virtual bool IsVSync() const override;

	private:
		void Init(const WindowProperties& props);
		void Shutdown();

	private:
		//GLFWwindow* m_Window;
		//GLFWcursor* m_Cursor = nullptr;
		//Scope<GraphicsContext> m_Context;

		struct WindowData
		{
			std::string Title;
			uint32_t Width, Height;
			bool VSync;
			EventCallbackFn EventCallback;
		};

		WindowData m_Data;
	};
}
#endif