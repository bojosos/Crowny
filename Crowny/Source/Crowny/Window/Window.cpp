#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Window/Window.h"

#ifdef CW_WINDOWS
#include "Platform/Windows/WindowsWindow.h"
#endif

#include "Platform/Linux/LinuxWindow.h"
#ifdef CW_LINUX
#endif

namespace Crowny
{

    Window* Window::Create(const WindowDesc& windowDesc)
    {
#ifdef CW_WINDOWS
        return new LinuxWindow(windowDesc);
#elif defined(CW_LINUX)
        // return CreateRef<LinuxWindow>(props);
        return new LinuxWindow(config);
#else
        CW_ENGINE_ASSERT(false, "Platform not supported");
        return nullptr;
#endif
    }

} // namespace Crowny