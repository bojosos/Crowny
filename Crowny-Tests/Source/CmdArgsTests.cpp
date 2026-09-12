#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/CmdArgs.h"

using namespace Crowny;

TEST_CASE("Command line arguments preserve values and option order", "[Application][CmdArgs]")
{
    const CommandLineArguments args(
      { "editor", "scene.cwscene", "--verbose", "--project", "C:/My Project", "-o=build", "--define=A=1", "--define", "B=2", "--empty=" });
    CHECK(args.GetExecutable() == "editor");
    CHECK(args.GetPositionals() == Vector<String>{ "scene.cwscene" });
    CHECK(args.HasOption("--verbose"));
    CHECK_FALSE(args.GetValue("--verbose").has_value());
    CHECK_FALSE(args.HasOption("--missing"));
    CHECK_FALSE(args.GetValue("--missing").has_value());
    CHECK(args.GetValue("--project") == "C:/My Project");
    CHECK(args.GetValue("-o") == "build");
    CHECK(args.GetValue("--define") == "B=2");
    CHECK(args.GetValues("--define") == Vector<String>{ "A=1", "B=2" });
    CHECK(args.GetValue("--empty") == "");
    CHECK(args.GetOptions().front().Name == "--verbose");
}

TEST_CASE("Command line terminator protects positional arguments", "[Application][CmdArgs]")
{
    const CommandLineArguments args({ "--not-an-option", "--flag", "--", "--render-api=vulkan", "-", "--", "file name" });
    CHECK(args.HasOption("--flag"));
    CHECK_FALSE(args.GetValue("--flag").has_value());
    CHECK_FALSE(args.HasOption("--render-api"));
    CHECK_FALSE(args.HasOption("--not-an-option"));
    CHECK(args.GetPositionals() == Vector<String>{ "--render-api=vulkan", "-", "--", "file name" });
}

TEST_CASE("Command line values handle negative numbers and dash prefixes", "[Application][CmdArgs]")
{
    const CommandLineArguments args(
      { "engine", "--offset", "-42", "--scale", "-.5", "--path=--file", "--stdin", "-", "--count=2", "--count", "--next" });
    CHECK(args.GetInteger("--offset") == -42);
    CHECK(args.GetValue("--scale") == "-.5");
    CHECK(args.GetValue("--path") == "--file");
    CHECK(args.GetValue("--stdin") == "-");
    CHECK_FALSE(args.GetValue("--count").has_value());
    CHECK(args.GetValues("--count") == Vector<String>{ "2" });
    CHECK(args.HasOption("--next"));
}

TEST_CASE("Command line integer conversion rejects invalid input", "[Application][CmdArgs]")
{
    for (const String& value : { "", "1.5", "12px", " 2", "2 ", "+2", "9223372036854775808", "-9223372036854775809" })
    {
        INFO(value);
        CHECK_FALSE(CommandLineArguments({ "engine", "--value=" + value }).GetInteger("--value").has_value());
    }
    CHECK(CommandLineArguments({ "engine", "--value=9223372036854775807" }).GetInteger("--value") == INT64_MAX);
    CHECK(CommandLineArguments({ "engine", "--value=-9223372036854775808" }).GetInteger("--value") == INT64_MIN);
    CHECK(CommandLineArguments({ "engine", "--value=0" }).GetInteger("--value") == 0);
    CHECK_FALSE(CommandLineArguments({ "engine" }).GetInteger("--value").has_value());
}

TEST_CASE("Command line parsing accepts empty input", "[Application][CmdArgs]")
{
    const CommandLineArguments args(Vector<String>{});
    CHECK(args.GetExecutable().empty());
    CHECK(args.GetOptions().empty());
    CHECK(args.GetPositionals().empty());
}

TEST_CASE("Declared flags leave following positional arguments intact", "[Application][CmdArgs]")
{
    const CommandLineArguments args({ "editor", "--verbose", "scene.cwscene", "--output", "image.bmp" }, { "--verbose" });
    CHECK(args.HasOption("--verbose"));
    CHECK_FALSE(args.GetValue("--verbose"));
    CHECK(args.GetPositionals() == Vector<String>{ "scene.cwscene" });
    CHECK(args.GetValue("--output") == "image.bmp");
}
