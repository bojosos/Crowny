#pragma once

#include "Crowny/Layers/Layer.h"

namespace Crowny
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void Begin();
		virtual void End();

		virtual void OnEvent(Event& event) override;
		void BlockEvents(bool block) { m_BlockEvents = block; }

	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;
	};
}
