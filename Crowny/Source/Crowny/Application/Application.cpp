#include "cwpch.h"

#include "Crowny/Application/Application.h"

#include "Crowny/Application/Initializer.h"
#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/ImGui/ImGuiOpenGLLayer.h"
#include "Crowny/ImGui/ImGuiVulkanLayer.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Renderer.h"

/*#ifdef MC_WEB
#include <emscripten/emscripten.h>
#endif*/

// NONONONONONO
#include <GLFW/glfw3.h>

namespace Crowny
{

    uint8_t Application::s_GLFWWindowCount = 0;

    static void DispatchMain(void* fp)
    {
        auto* func = (std::function<void()>*)fp;
        (*func)();
    }

    Application* Application::s_Instance = nullptr;

    Application::Application(const std::string& name)
    {
        CW_ENGINE_ASSERT(!s_Instance, "Application already exists!");
        s_Instance = this;
        RenderWindowProperties props;
        props.Title = name;
        props.Width = 1920;
        props.Height = 1080;

        if (s_GLFWWindowCount == 0)
        {
            int success = glfwInit();
            CW_ENGINE_ASSERT(success, "Could not initialize GLFW!");
        }
        Initializer::Init();

        m_Window = RenderWindow::Create(props);
        m_Window->GetWindow()->SetEventCallback(CW_BIND_EVENT_FN(Application::OnEvent));
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            m_ImGuiLayer = new ImGuiOpenGLLayer();
            break;
        case RenderAPI::API::Vulkan:
            m_ImGuiLayer = new ImGuiVulkanLayer();
            break;
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            m_ImGuiLayer = nullptr;
        }
        if (m_ImGuiLayer)
            PushOverlay(m_ImGuiLayer);
    }

    Application::~Application()
    {
        Renderer::Shutdown();
        Initializer::Shutdown();
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

    void Application::Exit() { m_Running = false; }

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
#ifdef MC_WEB
        std::function<void()> loop = [&]() {
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

                if (m_ImGuiLayer)
                {
                    m_ImGuiLayer->Begin();
                    {
                        for (Layer* layer : m_LayerStack)
                            layer->OnImGuiRender();
                    }
                    m_ImGuiLayer->End();
                }
            }

            Input::OnUpdate();
            m_Window->GetWindow()->OnUpdate();
            auto& rapi = RenderAPI::Get();
            rapi.SwapBuffers(m_Window);
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
} // namespace Crowny
