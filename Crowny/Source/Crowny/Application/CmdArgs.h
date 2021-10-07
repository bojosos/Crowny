#pragma once

namespace Crowny
{
    class CommandLineArgs
    {
    public:
        static void Create(int argc, char** argv);
        static Vector<String>& Get() { return Instance().m_Args; }

    private:
        static CommandLineArgs& Instance()
        {
            static CommandLineArgs instance;
            return instance;
        };

        CommandLineArgs();
        Vector<String> m_Args;
    };
} // namespace Crowny