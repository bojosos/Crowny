#include "cwpch.h"
#include "label.h"
#include "engine/gl/texture.h"
#include "engine/gl/renderer/batchrenderer2d.h"

namespace Crowny
{
	Label::Label(const std::string& text, const Ref<Font>& font, const Rectangle& bounds) : UIElement(bounds, nullptr, Color::White), m_Text(text), m_Font(font)
	{
		m_Bounds.Width = FontManager::GetWidth(m_Font, m_Text);
		m_Bounds.Height = FontManager::GetHeight(m_Font, m_Text);
		m_CancelClicks = true;
	}

	Label::Label(const std::string& text, const Ref<Font>& font, const Rectangle& bounds, Color color) : UIElement(bounds, nullptr, color), m_Text(text), m_Font(font)
	{
		m_Bounds.Width = FontManager::GetWidth(m_Font, m_Text);
		m_Bounds.Height = FontManager::GetHeight(m_Font, m_Text);
		m_CancelClicks = true;
	}

	void Label::SetText(const std::string& text)
	{
		m_Text = text;
		m_Bounds.Width = FontManager::GetWidth(m_Font, m_Text);
		m_Bounds.Height = FontManager::GetHeight(m_Font, m_Text);
		m_CancelClicks = true;
	}

	bool Label::OnMousePressed(MouseButtonPressedEvent& e)
	{
		MC_INFO("{0} text clicked!" + m_Text);
		return m_CancelClicks;
	}

	bool Label::OnMouseReleased(MouseButtonReleasedEvent& e)
	{
		MC_INFO("{0} text released!" + m_Text);
		return m_CancelClicks;
	}

	void Label::Submit(BatchRenderer2D* renderer)
	{
		renderer->DrawString(m_Text, m_Bounds.X, m_Bounds.Y, m_Font, m_Color);
		for (auto & child : m_Children)
		{
			child->Submit(renderer);
		}
	}

}