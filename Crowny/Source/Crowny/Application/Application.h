#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Layers/LayerStack.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Window/RenderWindow.h"

#include "Crowny/ImGui/ImGuiLayer.h"
#include "Crowny/Renderer/RenderThread.h"
#include "Crowny/Scripting/Managed/ManagedBackendSelection.h"

int main(int argc, char** argv);

namespace Crowny
{

    class Window;
    class EngineRuntime;
    struct ScriptConfig
    {
        ManagedBackendPreset Backend = ManagedBackendPreset::Mono;
        Path ProgramManifest;
        Path RuntimeRoot;
        bool EnableDebugging = false;
        bool EnableProfiling = false;
    };

    struct ApplicationDesc
    {
        RenderWindowDesc Window;
        Path WorkingDirectory = ".";
        Path InternalDirectory = "Internal";
        Path BuiltInResourcePackPath;
        String Name;

        bool Headless = false;
        bool DeferRuntimeServices = false;
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
        Time& GetTime() { return m_Time; }
        const Time& GetTime() const { return m_Time; }
        const Ref<RenderWindow>& GetRenderWindow() const { return m_Windows[0]; }
        Ref<TimeSettings> GetTimeSettings() const;
        void SetTimeSettings(const Ref<TimeSettings>& timeSettings);
        ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }
        RenderThread* GetRenderThread() const { return m_RenderThread.get(); }
        EngineRuntime& GetRuntime();
        const EngineRuntime& GetRuntime() const;
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
        double m_LastFrameTime = 0.0;
        Time m_Time;

        LayerStack* m_LayerStack;
        ImGuiLayer* m_ImGuiLayer;
        Scope<RenderThread> m_RenderThread;
        ApplicationDesc m_ApplicationDesc;
        Scope<EngineRuntime> m_Runtime;
    };

    void CreateApplication();
} // namespace Crowny
