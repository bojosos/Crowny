#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <cstdint>
#include <optional>

namespace Crowny
{
    // Parses an argv-style vector, including the executable at index zero.
    // Option names include their dash prefix. Short options are not bundled.
    class CommandLineArguments
    {
    public:
        struct Option
        {
            String Name;
            std::optional<String> Value;
        };

        explicit CommandLineArguments(const Vector<String>& arguments, const Vector<String>& flags = {});

        const String& GetExecutable() const { return m_Executable; }
        const Vector<Option>& GetOptions() const { return m_Options; }
        const Vector<String>& GetPositionals() const { return m_Positionals; }
        bool HasOption(StringView name) const;
        // The last occurrence wins. A missing option or a bare flag has no value.
        std::optional<String> GetValue(StringView name) const;
        Vector<String> GetValues(StringView name) const;
        // Rejects malformed and out-of-range integers without throwing.
        std::optional<int64_t> GetInteger(StringView name) const;

    private:
        String m_Executable;
        Vector<Option> m_Options;
        Vector<String> m_Positionals;
    };

    class CommandLineArgs
    {
    public:
        static void Create(int argc, char** argv);
        static Vector<String>& Get() { return Instance().m_Args; }
        // Return a snapshot so legacy mutations through Get() remain visible.
        static CommandLineArguments GetParsed(const Vector<String>& flags = {}) { return CommandLineArguments(Get(), flags); }

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
