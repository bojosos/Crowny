#include <Crowny.h>

#include <Crowny/EntryPoint.h>

#include "EditorLayer.h"

namespace Crowny
{
	class CrownyEditor : public Application
	{
	public:
		CrownyEditor() : Application("Crowny Editor")
		{
			PushLayer(new EditorLayer());
		}

	};

	Application* CreateApplication()
	{
		return new CrownyEditor();
	}
}