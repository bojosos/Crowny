#pragma once

#include "Crowny/ImGui/ImGuiLayer.h"

namespace Crowny
{
	class ImGuiOpenGLLayer : public ImGuiLayer
	{
	public:
		ImGuiOpenGLLayer();
		~ImGuiOpenGLLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void Begin() override;
		virtual void End() override;
	};
}