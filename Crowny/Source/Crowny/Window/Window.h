#pragma once

#include "Crowny/Events/Event.h"

namespace Crowny
{
	struct WindowProperties
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;

		WindowProperties(const std::string& title = "Crowny", uint32_t width = 1280, uint32_t height = 720) : Title(title), Width(width), Height(height)
		{

		}
	};

	enum class Cursor;

	class Window
	{
	public:
		virtual ~Window() = default;

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual bool GetVSync() const = 0;
		virtual const std::string& GetTitle() const = 0;

		virtual void SetCursor(Cursor cursor) = 0;

		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void* GetNativeWindow() const = 0;

		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

	public:
		static Window* Create(const WindowProperties& props = WindowProperties());
	};
}