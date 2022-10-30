#include "cwepch.h"

#include "EditorLayer.h"

#include <Crowny/Application/EntryPoint.h>

namespace Crowny
{

    class CrownyEditor : public Application
    {
    public:
        CrownyEditor(const Crowny::ApplicationDesc& applicationDesc) : Application(applicationDesc) { PushLayer(new EditorLayer()); }
    };

    Application* CreateApplication()
    {
        ApplicationDesc applicationDesc;
        applicationDesc.Name = "Crowny Editor";
        applicationDesc.Window.Title = "Crowny Editor";
        applicationDesc.Window.StartMaximized = true;
        applicationDesc.Window.HideUntilSwap = true;
        applicationDesc.Script.EnableDebugging = true;
        applicationDesc.Script.EnableProfiling = true;

        return new CrownyEditor(applicationDesc);
    }
} // namespace Crowny