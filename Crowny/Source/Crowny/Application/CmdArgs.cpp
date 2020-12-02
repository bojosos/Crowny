#include "cwpch.h"

#include "Crowny/Application/CmdArgs.h"

namespace Crowny
{
	CommandLineArgs::CommandLineArgs()
	{

	}

	void CommandLineArgs::Create(int argc, char** argv)
	{
		CW_ENGINE_ASSERT(Instance().m_Args.size() == 0, "Only one instance of command line object args is allowed!");
		Instance().m_Args = std::vector<std::string>(argv, argv + argc);
	}
}