#pragma once
#include <vector>
#include "Crowny/Events/Event.h"
#include "Crowny/Renderer/Texture.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Common/Color.h"
#include <glm/glm.hpp>

namespace Crowny
{
	class BatchRenderer2D;

	class UIElement
	{
	public:
		virtual ~UIElement() { for (auto& el : m_Children) delete el; }

		inline virtual void SetColor(Color color) { m_Color = color; }
		inline virtual const Rectangle GetBounds() const { return m_Bounds; }
		inline virtual const uint32_t GetColor() const { return m_Color; }
		inline virtual const std::vector<glm::vec2>& GetUV() const { return m_UV; }
		virtual void AddChild(UIElement* child);
		virtual void Fit(const Rectangle& bounds, const Padding& padding);

		virtual bool OnMousePressed(MouseButtonPressedEvent& e) = 0;
		virtual bool OnMouseReleased(MouseButtonReleasedEvent& e) = 0;
		virtual void Submit(BatchRenderer2D* renderer) = 0;

	protected:
		UIElement(const Rectangle& bounds);
		UIElement(const Rectangle& bounds, Color color);
		UIElement(const Rectangle& bounds, const Ref<Texture>& texture, Color color);
		UIElement() = delete;

		Color m_Color;
		std::vector<glm::vec2> m_UV;
		Ref<Texture> m_Texture;
		Rectangle m_Bounds;

		bool m_CancelClicks = false;
		UIElement* m_Parent = nullptr;
		std::vector<UIElement*> m_Children;

		void SetUVDefaults()
		{
			m_UV.push_back(glm::vec2(0, 0));
			m_UV.push_back(glm::vec2(0, 1));
			m_UV.push_back(glm::vec2(1, 1));
			m_UV.push_back(glm::vec2(1, 0));
		}

	};

	class Canvas : public UIElement
	{
	public:
		Canvas(uint32_t width, uint32_t height);
		void Submit(BatchRenderer2D* renderer) override;

		virtual bool OnMousePressed(MouseButtonPressedEvent& e) override;
		virtual bool OnMouseReleased(MouseButtonReleasedEvent& e) override;

	};
}
