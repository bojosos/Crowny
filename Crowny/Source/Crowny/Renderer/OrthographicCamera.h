#pragma once

#include "Crowny/Ecs/Components.h"

namespace Crowny
{
	class OrthographicCamera : public Camera
	{
	public:
		OrthographicCamera(const glm::mat4& orthographicProj);

		friend class ComponentEditor;
	public:
		//void RecalculateProjection(); 

	private:
		float m_Size;
	};
}