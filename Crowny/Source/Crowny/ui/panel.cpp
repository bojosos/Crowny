#include "cwpch.h"
#include "panel.h"
#include "engine/gl/renderer/batchrenderer2d.h"

namespace Crowny
{

	Panel::Panel() : UIElement(Rectangle(0.0f, 0.0f, 0.0f, 0.0f))
	{

	}

	Panel::Panel(Color color) : UIElement(Rectangle(0.0f, 0.0f, 0.0f, 0.0f), color)
	{

	}

	Panel::Panel(const Rectangle& bounds) : UIElement(bounds, Color::Transparent)
	{

	}

	Panel::Panel(const Rectangle& bounds, Color color) : UIElement(bounds, color)
	{

	}

	Panel::Panel(const Rectangle& bounds, const Ref<Texture>& texture, Color color) : UIElement(bounds, texture, color)
	{

	}

	void Panel::Submit(BatchRenderer2D* renderer)
	{
		renderer->FillRect(m_Bounds, m_Color);
		for (auto& child : m_Children) 
		{
			child->Submit(renderer);
		}
	}

	bool Panel::OnMousePressed(MouseButtonPressedEvent& e)
	{
		for (auto& child : m_Children)
		{
			child->OnMousePressed(e);
		}
		return m_CancelClicks;
	}

	bool Panel::OnMouseReleased(MouseButtonReleasedEvent& e)
	{
		for (auto& child : m_Children)
		{
			child->OnMouseReleased(e);
		}
		return m_CancelClicks;
	}

}