#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Common/Timestep.h"

#include "Crowny/Events/Event.h"

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
		virtual void OnImGuiRender() {};

		virtual void OnEvent(Event& event) {}

		const std::string& GetName() const { return m_Name; }
	private:
		std::string m_Name;
	};
}