#include "cwpch.h"
#include "Crowny/Window/Window.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Events/ApplicationEvent.h"

#include "Platform/Windows/WindowsWindow.h"

namespace Crowny
{
	
	Scope<Window> Window::Create(const WindowProperties& props)
	{
		return CreateScope<WindowsWindow>(props);
	}

}
