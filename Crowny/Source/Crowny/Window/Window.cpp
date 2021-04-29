#include "cwpch.h"

#include "Crowny/Window/Window.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Input/Input.h"

#ifdef CW_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsWindow.h"
#endif

#ifdef CW_PLATFORM_LINUX
#include "Platform/Linux/LinuxWindow.h"
#endif

namespace Crowny
{
	
	Scope<Window> Window::Create(const WindowProperties& props)
	{
	#ifdef CW_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
	#elif defined (CW_PLATFORM_LINUX)
		return CreateScope<LinuxWindow>(props);
	#else
		CW_ENGINE_ASSERT(false, "Platform not supported");
		return nullptr;
	#endif
	}

}