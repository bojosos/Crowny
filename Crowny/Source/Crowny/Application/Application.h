#pragma once

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Layers/LayerStack.h"
#include "Crowny/Window/RenderWindow.h"

#include "Crowny/ImGui/ImGuiLayer.h"

#include <glm/glm.hpp>

int main(int argc, char** argv);

namespace Crowny
{
    class Application
    {
    public:
        Application(const String& name);
        ~Application();

        void OnEvent(Event& event);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        Window& GetWindow() const { return *m_Window->GetWindow(); }
        const Ref<RenderWindow>& GetRenderWindow() const { return m_Window; }
        ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
        void Exit();

        static Application& Get() { return *s_Instance; }

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);

        void Run();

    private:
        Ref<RenderWindow> m_Window;
        bool m_Running = true;
        bool m_Minimized = false;
        float m_LastFrameTime = 0.0f;

        LayerStack* m_LayerStack;
        ImGuiLayer* m_ImGuiLayer;

    private:
        static Application* s_Instance;
        friend int ::main(int argc, char** argv);

    public:
        static uint8_t s_GLFWWindowCount;
    };

    Application* CreateApplication();
} // namespace Crowny
