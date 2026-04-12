#include "cwepch.h"

#include "EditorLayer.h"

#include <Crowny/Application/EntryPoint.h>
#include <Crowny/Utils/BuiltInShaderCompiler.h>

namespace Crowny
{

    class CrownyEditor : public Application
    {
    public:
        CrownyEditor(const Crowny::ApplicationDesc& applicationDesc) : Application(applicationDesc) {}

        virtual void OnPreRendererInit() override { BuiltInShaderCompiler::CompileAll(); }

        virtual void OnStartUp() override
        {
            Application::OnStartUp();
            PushLayer(new EditorLayer());
        }
    };

    void CreateApplication()
    {
        ApplicationDesc applicationDesc;
        applicationDesc.Name = "Crowny Editor";
        applicationDesc.Window.Title = "Crowny Editor";
        applicationDesc.Window.StartMaximized = true;
        applicationDesc.Window.HideUntilSwap = true;
        applicationDesc.Script.EnableDebugging = true;
        applicationDesc.Script.EnableProfiling = true;

        applicationDesc.WorkingDirectory = "C:\\\\dev\\\\Crowny";
        applicationDesc.EngineAssemblyPath = "Crowny-Sharp/CrownySharp.dll";
        applicationDesc.GameAssemblyPath = "Crowny-Sandbox/GameAssembly.dll";

        Application::StartUp<CrownyEditor>(applicationDesc);
    }
} // namespace Crowny