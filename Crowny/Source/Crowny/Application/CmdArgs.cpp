#include "cwpch.h"

#include "Crowny/Application/CmdArgs.h"

#include <charconv>

namespace Crowny
{
    namespace
    {
        bool IsOption(StringView argument)
        {
            return argument.size() > 1 && argument[0] == '-' && !(argument[1] >= '0' && argument[1] <= '9') &&
                   !(argument[1] == '.' && argument.size() > 2 && argument[2] >= '0' && argument[2] <= '9');
        }
    } // namespace

    CommandLineArguments::CommandLineArguments(const Vector<String>& arguments, const Vector<String>& flags)
    {
        if (arguments.empty())
            return;
        m_Executable = arguments.front();
        bool positionalOnly = false;
        for (size_t index = 1; index < arguments.size(); ++index)
        {
            const String& argument = arguments[index];
            if (!positionalOnly && argument == "--")
            {
                positionalOnly = true;
                continue;
            }
            if (positionalOnly || !IsOption(argument))
            {
                m_Positionals.push_back(argument);
                continue;
            }

            const size_t equals = argument.find('=');
            Option option{ argument.substr(0, equals), std::nullopt };
            if (equals != String::npos)
                option.Value = argument.substr(equals + 1);
            else if (std::find(flags.begin(), flags.end(), option.Name) == flags.end() && index + 1 < arguments.size() &&
                     !IsOption(arguments[index + 1]))
                option.Value = arguments[++index];
            m_Options.push_back(std::move(option));
        }
    }

    bool CommandLineArguments::HasOption(StringView name) const
    {
        return std::any_of(m_Options.begin(), m_Options.end(), [&](const Option& option) { return option.Name == name; });
    }

    std::optional<String> CommandLineArguments::GetValue(StringView name) const
    {
        for (auto option = m_Options.rbegin(); option != m_Options.rend(); ++option)
            if (option->Name == name)
                return option->Value;
        return std::nullopt;
    }

    Vector<String> CommandLineArguments::GetValues(StringView name) const
    {
        Vector<String> values;
        for (const Option& option : m_Options)
            if (option.Name == name && option.Value)
                values.push_back(*option.Value);
        return values;
    }

    std::optional<int64_t> CommandLineArguments::GetInteger(StringView name) const
    {
        const auto value = GetValue(name);
        if (!value)
            return std::nullopt;
        int64_t result = 0;
        const auto parsed = std::from_chars(value->data(), value->data() + value->size(), result);
        if (parsed.ec != std::errc() || parsed.ptr != value->data() + value->size())
            return std::nullopt;
        return result;
    }

    CommandLineArgs::CommandLineArgs() {}

    void CommandLineArgs::Create(int argc, char** argv)
    {
        CW_ENGINE_ASSERT(Instance().m_Args.size() == 0, "Only one instance of command line object args is allowed!");
        if (argc > 0 && argv != nullptr)
            Instance().m_Args = Vector<String>(argv, argv + argc);
    }
} // namespace Crowny
