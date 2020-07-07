#pragma once

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Layers/LayerStack.h"
#include "Crowny/Window/Window.h"

#include "Crowny/ImGui/ImGuiLayer.h"

#include <glm/glm.hpp>

namespace Crowny
{
	class Application
	{
	public:
		Application(const std::string& name);
		~Application() = default;

		void OnEvent(Event& event);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		void Run();

		inline Window& GetWindow() const { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }
		inline static uint32_t GetWidth() { return Get().GetWindow().GetWidth(); }
		inline static uint32_t GetHeight() { return Get().GetWindow().GetHeight(); }

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;

		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer;

	private:
		static Application* s_Instance;
	};

	Application* CreateApplication();
}
