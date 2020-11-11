#pragma once

#include <glm/glm.hpp>
#include "Vector3.h"

#include "Crowny-CLI.h"

namespace Crowny
{
	public ref class Vector3 : public ManagedClass<glm::mat4>
	{
	private:
		Vector3(glm::mat4* instance);
	public:
		property Vector3 Position
		{
			Vector3^ Get()
			{
				return gcnew Vector3(m_Instance[3][0], m_Instance[3][1], m_Instance[3][2]);
			}

			void Set(Vector3 value)
			{
				
			}
		}

	};
}