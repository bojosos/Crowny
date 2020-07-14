#pragma once

namespace Crowny
{
	class CommandLineArgs
	{
	public:
		static void Create(int argc, char** argv);
		inline static std::vector<std::string>& Get() { return Instance().m_Args; }
	private:
		static CommandLineArgs& Instance()
		{
			static CommandLineArgs instance;
			return instance;
		};

		CommandLineArgs();
		std::vector<std::string> m_Args;
	};
}