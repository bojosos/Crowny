#pragma once

#include "Crowny/Layers/Layer.h"

struct ImFont;

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

		ImFont* GetFontAwesomeFont() const { return m_FontAwesomeFont; }

    private:
		ImFont* m_FontAwesomeFont;
        bool m_BlockEvents = true;
        float m_Time = 0.0f;
    };
} // namespace Crowny
