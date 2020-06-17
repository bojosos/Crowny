#include "cwpch.h"

#include "Crowny/Application.h"

#include "Crowny/Common/Log.h"
#include "Crowny/Common/Common.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Font.h"

#ifdef MC_WEB
#include <emscripten/emscripten.h>
#endif

<<<<<<< HEAD
// NONONONONONO
#include <GLFW/glfw3.h>

=======
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
namespace Crowny
{

	static void DispatchMain(void* fp)
	{
<<<<<<< HEAD
		auto* func = (std::function<void()>*)fp;
=======
		std::function<void()>* func = (std::function<void()>*)fp;
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
		(*func)();
	}

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		CW_ENGINE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;
		m_Window = Window::Create();

		m_Window->SetEventCallback(CW_BIND_EVENT_FN(Application::OnEvent));
		FontManager::Add(CreateRef<Font>("default", DEFAULT_FONT_PATH, 16));
	}

	Application::~Application()
	{

	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CW_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(CW_BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			(*it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
<<<<<<< HEAD
=======
		//Application::Get().GetWindow().SetVSync(false);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
#ifdef MC_WEB
		std::function<void()> loop = [&]() 
		{
#else
		while (m_Running)
		{
#endif
			float time = (float)glfwGetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			if (!m_Minimized)
			{
				for (Layer* layer : m_LayerStack)
					layer->OnUpdate(timestep);
			}
			/*
			m_ImGuiLayer->Begin();
			{
				HZ_PROFILE_SCOPE("LayerStack OnImGuiRender");

				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRender();
		}
			m_ImGuiLayer->End();
			*/
			m_Window->OnUpdate();
#ifdef MC_WEB
		};
		emscripten_set_main_loop_arg(DispatchMain, &loop, 0, 1);
#else
		}
#endif
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}
	
	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		return false;
	}
}