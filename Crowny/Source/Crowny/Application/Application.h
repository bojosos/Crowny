#pragma once

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Layers/LayerStack.h"
#include "Crowny/Window/RenderWindow.h"

#include "Crowny/ImGui/ImGuiLayer.h"

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
        String WorkingDirectory = ".";
        String Name;

        ScriptConfig Script;
    };

    class Application
    {
    public:
        Application(const ApplicationDesc& applicationDesc);
        ~Application();

        void OnEvent(Event& event);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        Window& GetWindow() const;
        const Ref<RenderWindow>& GetRenderWindow() const { return m_Window; }
        Ref<TimeSettings> GetTimeSettings() const;
        void SetTimeSettings(const Ref<TimeSettings>& timeSettings);
        ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
        void Exit();
        const ApplicationDesc& GetApplicationDesc() const { return m_Desc; }

        static Application& Get() { return *s_Instance; }

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);
        bool OnWindowMinimized(WindowMinimizeEvent& e);
        bool OnMouseScroll(MouseScrolledEvent& event);

        void Run();

    private:
        Ref<RenderWindow> m_Window;
        Ref<TimeSettings> m_TimeSettings;
        bool m_Running = true;
        bool m_Minimized = false;
        float m_LastFrameTime = 0.0f;

        LayerStack* m_LayerStack;
        ImGuiLayer* m_ImGuiLayer;
        ApplicationDesc m_Desc;

    private:
        static Application* s_Instance;
        friend int ::main(int argc, char** argv);

    public:
        static uint8_t s_GLFWWindowCount;
    };

    Application* CreateApplication();
} // namespace Crowny
