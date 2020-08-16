#include <Crowny.h>

#include <Crowny/Application/EntryPoint.h>

#include "EditorLayer.h"

namespace Crowny
{

	class CrownyEditor : public Application
	{
	public:
		CrownyEditor() : Application("Crowny Editor")
		{
			char c = 'a';
			CW_ENGINE_INFO(sizeof(c));
			PushLayer(new EditorLayer());
		}
	};

	Application* CreateApplication()
	{
		return new CrownyEditor();
	}
}