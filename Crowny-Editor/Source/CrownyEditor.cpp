#include <Crowny.h>

#include "EditorLayer.h"

namespace Crowny
{
	class CrownyEditor : public Application
	{
	public:
		CrownyEditor() : Application("Crowny Editr")
		{
			PushLayer(new EditorLayer());
		}

	};

	Application* CreateApplication()
	{
		return new CrownyEditor();
	}
}