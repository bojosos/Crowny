#include "cwpch.h"

#include "Crowny/Runtime/Runtime.h"

namespace Crowny
{

	Runtime::Runtime()
	{

	}

	void Runtime::OnSceneChanged(const Ref<Scene>& scene)
	{
		m_OpenScene = scene;
	}

	void Runtime::OnStartup()
	{
		//m_Scene = new Scene(*m_OpenScene);
	}

	void Runtime::OnShutdown()
	{
		m_Scene = nullptr;
	}

}