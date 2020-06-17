#pragma once
#include "element.h"
#include "font.h"

namespace Crowny
{
	class Label : public UIElement
	{
	public:
		Label(const std::string& text, const Ref<Font>& font, const Rectangle& bounds);
		Label(const std::string& text, const Ref<Font>& font, const Rectangle& bounds, Color color);

		virtual bool OnMousePressed(MouseButtonPressedEvent& e) override;
		virtual bool OnMouseReleased(MouseButtonReleasedEvent& e) override;

		const inline std::string& GetText() const { return m_Text; }
		void SetText(const std::string& text);

		void Submit(BatchRenderer2D* renderer) override;

	private:
		std::string m_Text;
		Ref<Font> m_Font;
	};
}