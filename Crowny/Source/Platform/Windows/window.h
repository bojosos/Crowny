#pragma once
#include "common/common.h"
#include "common/event/event.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace Crowny
{
	struct WindowProps
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;

		WindowProps(const std::string& title = "Minecraft", uint32_t width = 1280, uint32_t height = 720) : Title(title), Width(width), Height(height)
		{

		}
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Window(const WindowProps& props);
		~Window();
		void OnUpdate();

		inline uint32_t GetWidth() const { return m_Data.Width; };
		inline uint32_t GetHeight() const { return m_Data.Height; }
		inline glm::vec2 GetSize() const { return glm::vec2(m_Data.Width, m_Data.Height); }

		void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; };
		inline GLFWwindow* GetGLFWwindow() const { return m_Window; }

		void SetVSync(bool enabled);
		bool IsVSync() const;

		//GL
		void InitContext();
		void SwapBuffers();

	public:
		static Scope<Window> Create(const WindowProps& props = WindowProps());

	private:
		void Init(const WindowProps& props);
		void Shutdown();

	private:
		GLFWwindow* m_Window;

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
