#include "cwpch.h"
#include "element.h"
#include "engine/gl/renderer/batchrenderer2d.h"
#include "engine/application.h"

namespace Crowny
{

	UIElement::UIElement(const Rectangle& bounds) : m_Bounds(bounds), m_Color(Color::White)
	{
		//m_Bounds.Y = Application::GetHeight() - m_Bounds.Y;
	}

	UIElement::UIElement(const Rectangle& bounds, Color color) : m_Bounds(bounds), m_Color(color)
	{

	}

	UIElement::UIElement(const Rectangle& bounds, const Ref<Texture>& texture, Color color) : m_Bounds(bounds), m_Color(color), m_Texture(texture)
	{
		//m_Bounds.Y = Application::GetHeight() - m_Bounds.Y;
	}

	void UIElement::AddChild(UIElement* child)
	{
		m_Children.push_back(child);
	}

	void UIElement::Fit(const Rectangle& bounds, const Padding& padding)
	{
		m_Bounds = Rectangle(bounds.X - padding.Left, bounds.Y - padding.Top, bounds.Width + padding.Left + padding.Right, bounds.Height + padding.Top + padding.Bottom);
	}

	Canvas::Canvas(uint32_t width, uint32_t height) : UIElement(Rectangle(0, 0, width, height))
	{
		
	}

	void Canvas::Submit(BatchRenderer2D* renderer)
	{
		for (auto& child : m_Children)
		{
			child->Submit(renderer);
		}
	}

	bool Canvas::OnMousePressed(MouseButtonPressedEvent& e)
	{
		for (auto& child : m_Children)
		{
			if (m_Bounds.Contains(e.GetPosition()))
			{
				return child->OnMousePressed(e);
			}
		}
	}

	bool Canvas::OnMouseReleased(MouseButtonReleasedEvent& e)
	{
		for (auto& child : m_Children)
		{
			if (m_Bounds.Contains(e.GetPosition()))
			{
				return child->OnMouseReleased(e);
			}
		}
	}
}
