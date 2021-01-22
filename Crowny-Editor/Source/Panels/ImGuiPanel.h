#pragma once

namespace Crowny
{
	class ImGuiPanel
	{
	public:
		ImGuiPanel(const std::string& name);
		virtual ~ImGuiPanel() = default;

		virtual void Show() { m_Shown = true; };
		virtual void Hide() { m_Shown = false; };

		virtual void Render() = 0;
		virtual const std::string& GetName() const { return m_Name; }
		virtual bool IsFocused() { return m_Focused; }
		virtual bool IsHovered() { return m_Hovered; }
	protected:
		void UpdateState();
	protected:
		bool m_Focused = false, m_Hovered = false;
		std::string m_Name;
		bool m_Shown;
	};
}
