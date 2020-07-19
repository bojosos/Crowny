#pragma once

namespace Crowny
{
	class ImGuiWindow
	{
	public:
		ImGuiWindow(const std::string& name);
		virtual ~ImGuiWindow() = default;

		virtual void Show() = 0;
		virtual void Hide() = 0;

		virtual void Render() = 0;
		virtual const std::string& GetName() const { return m_Name; }
	protected:
		std::string m_Name;
		bool m_Shown;
	};
}