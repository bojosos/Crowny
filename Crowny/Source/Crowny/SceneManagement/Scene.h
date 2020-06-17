#pragma once

#include "Crowny/Components/Entity.h"

namespace Crowny
{
	class Scene
	{
	private:
		std::vector<Ref<Entity>> m_Entities;
		uint32_t m_BuildIndex;
		std::string m_Name;
		std::string m_Filepath;
		bool m_IsLoaded;

		Scene(const std::string& name);
	public:
		void AddEntity(const Ref<Entity>& entity);
	};
}
