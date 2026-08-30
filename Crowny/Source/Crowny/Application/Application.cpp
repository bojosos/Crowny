#include "cwpch.h"

#include "Crowny/Application/Application.h"

#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/ImGui/ImGuiOpenGLLayer.h"
#include "Crowny/ImGui/ImGuiVulkanLayer.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Window/RenderWindow.h"

#include <tracy/Tracy.hpp>

#include <GLFW/glfw3.h>
#include <stdexcept>

namespace Crowny
{

    static void DispatchMain(void* fp)
    {
        auto* const func = (std::function<void()>*)fp;
        (*func)();
    }

    Application::Application(const ApplicationDesc& applicationDesc) : m_ApplicationDesc(applicationDesc) { m_LayerStack = new LayerStack(); }

    Application::~Application() { delete m_LayerStack; }

    void Application::OnStartUp()
    {
        m_Runtime = CreateScope<EngineRuntime>(m_ApplicationDesc);
        if (m_ApplicationDesc.Headless)
        {
            m_Runtime->Start();
            return;
        }

        if (!Window::Initialize())
            throw std::runtime_error("Could not initialize the desktop window system");
        m_Runtime->Start();

        Ref<RenderWindow> mainWindow = RenderWindow::Create(m_ApplicationDesc.Window);
        if (!mainWindow || !mainWindow->GetWindow())
            throw std::runtime_error("Could not create the main render window");
        mainWindow->GetWindow()->SetEventCallback(CW_BIND_EVENT_FN(Application::OnEvent));
        m_Windows.push_back(mainWindow);
        m_Runtime->StartRenderer();
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            m_ImGuiLayer = new ImGuiOpenGLLayer();
            break;
        case RenderAPI::API::Vulkan:
            m_ImGuiLayer = new ImGuiVulkanLayer();
            break;
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            m_ImGuiLayer = nullptr;
        }
        if (m_ImGuiLayer)
            PushOverlay(m_ImGuiLayer);

        if (RenderAPI::TryGet()->GetAPI() == RenderAPI::API::Vulkan)
        {
            m_RenderThread = CreateScope<RenderThread>();
            m_RenderThread->Start();
        }
        else
        {
            CW_ENGINE_INFO("OpenGL rendering is using the main thread because its context is thread-affine");
        }
    }
    void Application::OnShutdown()
    {
        const RenderAPI::API activeRenderAPI = RenderAPI::GetAPI();
        if (m_RenderThread)
        {
            m_RenderThread->Stop();
            m_RenderThread.reset();
        }
        // OpenGL and renderer fallback paths build the same thread-local scene
        // caches on the main thread. Release them while the active RenderAPI and
        // device are still valid; this is a no-op when that thread has no cache.
        SceneRenderer::ShutdownRenderThreadResources();

        if (!m_ApplicationDesc.Headless)
        {
            m_Runtime->StopRenderer();

            // Destroy all layers (ImGui, EditorLayer, etc.) BEFORE shutting down the
            // rendering subsystems. Layers hold GPU resources (render targets, materials,
            // textures, command buffers) that must be released while the Vulkan device is
            // still alive. Module::Shutdown() calls OnShutdown() then deletes the instance,
            // so ~Application() runs after the device is gone — too late for GPU cleanup.
            // Nulling the pointer prevents the double-free in ~Application().
            delete m_LayerStack;
            m_LayerStack = nullptr;

            m_Runtime->ShutdownRendererResources();
        }
        m_Runtime->ShutdownServices();
        m_Runtime->ShutdownCoreServices();
        if (!m_ApplicationDesc.Headless)
        {
            if (activeRenderAPI == RenderAPI::API::OpenGL)
            {
                m_Runtime->ShutdownRenderAPI();
                m_Windows.clear();
            }
            else
            {
                m_Windows.clear();
                m_Runtime->ShutdownRenderAPI();
            }
            Window::Shutdown();
        }
        else
        {
            m_Runtime->ShutdownRenderAPI();
        }
        m_Runtime.reset();
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack->PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* layer)
    {
        m_LayerStack->PushOverlay(layer);
        layer->OnAttach();
    }

    void Application::Exit() { m_Running = false; }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowMinimizeEvent>(CW_BIND_EVENT_FN(Application::OnWindowMinimized));
        dispatcher.Dispatch<WindowResizeEvent>(CW_BIND_EVENT_FN(Application::OnWindowResize));
        dispatcher.Dispatch<MouseScrolledEvent>(CW_BIND_EVENT_FN(Application::OnMouseScroll));

        for (auto it = m_LayerStack->rbegin(); it != m_LayerStack->rend(); ++it)
        {
            (*it)->OnEvent(e);
            if (e.Handled)
                break;
        }

        if (e.GetEventType() == EventType::WindowClose)
            OnWindowClose(static_cast<WindowCloseEvent&>(e));
    }

    Ref<TimeSettings> Application::GetTimeSettings() const
    {
        if (!m_TimeSettings)
        {
            static Ref<TimeSettings> defaultTimeSettings = CreateRef<TimeSettings>();
            return defaultTimeSettings;
        }
        return m_TimeSettings;
    }
    void Application::SetTimeSettings(const Ref<TimeSettings>& timeSettings) { m_TimeSettings = timeSettings; }

    EngineRuntime& Application::GetRuntime()
    {
        CW_ENGINE_ASSERT(m_Runtime != nullptr);
        return *m_Runtime;
    }

    const EngineRuntime& Application::GetRuntime() const
    {
        CW_ENGINE_ASSERT(m_Runtime != nullptr);
        return *m_Runtime;
    }

    Window& Application::GetWindow() const { return *m_Windows[0]->GetWindow(); }

    void Application::Run()
    {
        m_LastFrameTime = static_cast<float>(glfwGetTime());
#ifdef MC_WEB
        std::function<void()> loop = [&]() {
#else
        while (m_Running)
        {
#endif
            const float time = (float)glfwGetTime();
            const Timestep timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;
            auto& rapi = *RenderAPI::TryGet();
            rapi.BeginFrameStatistics(timestep.GetSeconds());

            Input::BeginFrame();
            Window::PollEvents();
            for (const Ref<RenderWindow>& window : m_Windows)
                window->GetWindow()->OnUpdate();
            Input::UpdateGamepads();
            Input::UpdateActions();
            if (!m_Windows.empty() && m_Windows.front()->GetWindow()->ShouldClose())
                m_Running = false;
            if (!m_Running)
                break;

            AssetListenerManager::Get().Update();
            if (AudioManager::IsStartedUp())
                AudioManager::Get().OnUpdate();

            if (!m_Minimized)
            {
                {
                    ZoneScopedN("LayerUpdates");
                    for (Layer* layer : *m_LayerStack)
                        layer->OnUpdate(timestep);
                }

                if (m_ImGuiLayer)
                {
                    ZoneScopedN("ImGui");
                    m_ImGuiLayer->Begin();
                    {
                        for (Layer* layer : *m_LayerStack)
                            layer->OnImGuiRender();
                    }
                    m_ImGuiLayer->End();
                }
            }

            for (const Ref<RenderWindow>& window : m_Windows)
            {
                rapi.SwapBuffers(window);
            }
#ifdef MC_WEB
        };
        emscripten_set_main_loop_arg(DispatchMain, &loop, 0, 1);
#else
            FrameMark;
        }
#endif
    }

    bool Application::OnMouseScroll(MouseScrolledEvent& event)
    {
        Input::OnMouseScroll(event.GetXOffset(), event.GetYOffset());
        return false;
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        if (e.IsCancelled())
            return false;
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& e)
    {
        if (e.GetFramebufferWidth() == 0 || e.GetFramebufferHeight() == 0)
        {
            m_Minimized = true;
            return false;
        }

        m_Minimized = false;
        Renderer::OnWindowResize(e.GetFramebufferWidth(), e.GetFramebufferHeight());
        return false;
    }

    bool Application::OnWindowMinimized(WindowMinimizeEvent& e)
    {
        m_Minimized = e.IsMinimized();
        return false;
    }
} // namespace Crowny
