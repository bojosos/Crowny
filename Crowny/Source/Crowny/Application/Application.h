#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Layers/LayerStack.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Window/RenderWindow.h"

#include "Crowny/ImGui/ImGuiLayer.h"
#include "Crowny/Renderer/RenderThread.h"

int main(int argc, char** argv);

namespace Crowny
{

    class Window;
    struct TimeSettings;

    struct ScriptConfig
    {
        bool EnableDebugging = false;
        bool EnableProfiling = false;
    };

    struct ApplicationDesc
    {
        RenderWindowDesc Window;
        Path WorkingDirectory = ".";
        Path InternalDirectory = "Internal";
        String Name;

        bool Headless = false;
        bool EnableRayTracing = false;
        RenderAPI::API PreferredAPI = RenderAPI::API::Vulkan;

        Path EngineAssemblyPath;
        Path GameAssemblyPath;

        ScriptConfig Script;
    };

    class Application : public Module<Application>
    {
    public:
        Application(const ApplicationDesc& applicationDesc);
        ~Application();

        void OnEvent(Event& event);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        Window& GetWindow() const;
        const Ref<RenderWindow>& GetRenderWindow() const { return m_Windows[0]; }
        Ref<TimeSettings> GetTimeSettings() const;
        void SetTimeSettings(const Ref<TimeSettings>& timeSettings);
        ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }
        RenderThread* GetRenderThread() const { return m_RenderThread.get(); }
        bool IsMultiThreaded() const { return m_RenderThread != nullptr; }
        void Exit();
        const ApplicationDesc& GetApplicationDesc() const { return m_ApplicationDesc; }
        const Path& GetWorkingDirectory() const { return m_ApplicationDesc.WorkingDirectory; }
        const Path& GetInternalDirectory() const { return m_ApplicationDesc.InternalDirectory; }
        void SetInternalDirectory(const Path& path) { m_ApplicationDesc.InternalDirectory = path; }

        void Run();

        virtual void OnPreRendererInit() {}

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);
        bool OnWindowMinimized(WindowMinimizeEvent& e);
        bool OnMouseScroll(MouseScrolledEvent& event);

    protected:
        virtual void OnStartUp() override;
        virtual void OnShutdown() override;

    private:
        Vector<Ref<RenderWindow>> m_Windows;
        Ref<TimeSettings> m_TimeSettings;
        bool m_Running = true;
        bool m_Minimized = false;
        float m_LastFrameTime = 0.0f;

        LayerStack* m_LayerStack;
        ImGuiLayer* m_ImGuiLayer;
        Scope<RenderThread> m_RenderThread;
        ApplicationDesc m_ApplicationDesc;

    public:
        static uint8_t s_GLFWWindowCount;
    };

    void CreateApplication();

    extern Application* gApplication;
} // namespace Crowny
