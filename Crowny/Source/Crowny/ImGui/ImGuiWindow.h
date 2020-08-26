#pragma once

namespace Crowny
{
	class ImGuiWindow
	{
	public:
		ImGuiWindow(const std::string& name);
		virtual ~ImGuiWindow() = default;

		virtual void Show() { m_Shown = true; };
		virtual void Hide() { m_Shown = false; };

		virtual void Render() = 0;
		virtual const std::string& GetName() const { return m_Name; }
	protected:
		std::string m_Name;
		bool m_Shown;
	};
}