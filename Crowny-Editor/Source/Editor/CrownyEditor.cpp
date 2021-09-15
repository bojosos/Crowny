#include "cwepch.h"

#include "EditorLayer.h"

#include <Crowny/Application/EntryPoint.h>

namespace Crowny
{

    class CrownyEditor : public Application
    {
    public:
        CrownyEditor() : Application("Crowny Editor") { PushLayer(new EditorLayer()); }
    };

    Application* CreateApplication() { return new CrownyEditor(); }
} // namespace Crowny