#pragma once

#include "element.h"
#include "engine/gl/texture.h"
#include "common/color.h"

namespace Crowny
{
	class Panel : public UIElement
	{
	public:
		Panel();
		Panel(Color color);
		Panel(const Rectangle& bounds);
		Panel(const Rectangle& bounds, Color color);
		Panel(const Rectangle& bounds, const Ref<Texture>& texture, Color color);

		void Submit(BatchRenderer2D* renderer) override;
		bool OnMousePressed(MouseButtonPressedEvent& e) override;
		bool OnMouseReleased(MouseButtonReleasedEvent& e) override;
	};
}