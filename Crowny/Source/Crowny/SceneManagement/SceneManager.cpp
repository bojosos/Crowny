#include "cwpch.h"

#include "Crowny/SceneManagement/SceneManager.h"

namespace Crowny
{

	Ref<Scene> SceneManager::CreateScene(const std::string& name)
	{
<<<<<<< HEAD
		//return CreateRef<Scene>(name);
		return nullptr;
=======
		return CreateRef<Scene>(name);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
	}

}