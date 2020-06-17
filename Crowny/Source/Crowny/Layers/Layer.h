#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Events/Event.h"
#include "Crowny/Common/Common.h"

namespace Crowny
{
	class Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnEvent(Event& event) {}

		inline const std::string& GetName() const { return m_Name; }
	private:
		std::string m_Name;
	};
}