#pragma once

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Layers/LayerStack.h"
<<<<<<< HEAD
#include "Crowny/Window/Window.h"

=======
#include "Crowny/window/Window.h"
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
#include <glm/glm.hpp>

namespace Crowny
{
	class Application
	{
	public:
		Application();
<<<<<<< HEAD
		~Application() = default;
=======
		~Application();
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2

		void OnEvent(Event& event);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		void Run();

<<<<<<< HEAD
=======
		inline glm::vec2 GetWindowSize() const { return m_Window->GetSize(); }
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
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
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;

	private:
		static Application* s_Instance;
	};
}
