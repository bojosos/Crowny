#pragma once

#include "Crowny/Application/CmdArgs.h"

namespace Crowny
{
    struct EditorLaunchOptions
    {
        Path Project;
        Path Scene;
        Path RenderOutput;
        uint32_t Width = 1280;
        uint32_t Height = 720;
        uint32_t Frames = 3;
        bool Play = false;
        bool Quit = false;
        bool Help = false;
        bool SceneCamera = false;
    };

    inline bool ParseEditorLaunchOptions(const Vector<String>& arguments, const Path& launchDirectory, EditorLaunchOptions& result, String& error)
    {
        result = {};
        error.clear();
        const Vector<String> flags = { "--help", "-h", "--play", "--quit", "--scene-camera", "--opengl", "--vulkan", "--cook-builtins" };
        const CommandLineArguments args(arguments, flags);
        if (args.HasOption("--help") || args.HasOption("-h"))
        {
            result.Help = true;
            return true;
        }
        for (const auto& option : args.GetOptions())
        {
            const bool flag = std::find(flags.begin(), flags.end(), option.Name) != flags.end();
            if (flag)
            {
                if (!option.Value)
                    continue;
                error = option.Name + " does not accept a value";
                return false;
            }
            if (option.Name != "--project" && option.Name != "--scene" && option.Name != "--render" && option.Name != "--width" &&
                option.Name != "--height" && option.Name != "--frames" && option.Name != "--render-api")
            {
                error = "Unknown option: " + option.Name;
                return false;
            }
            if (!option.Value || option.Value->empty())
            {
                error = "Expected a value for " + option.Name;
                return false;
            }
        }
        if (!args.GetPositionals().empty())
        {
            error = "Unexpected positional argument: " + args.GetPositionals().front();
            return false;
        }
        auto absolutePath = [](const Path& base, const String& value) { return (base / Path(value)).lexically_normal(); };
        if (auto value = args.GetValue("--project"))
            result.Project = absolutePath(launchDirectory, *value);
        if (auto value = args.GetValue("--scene"))
            result.Scene = absolutePath(result.Project.empty() ? launchDirectory : result.Project, *value);
        if (auto value = args.GetValue("--render"))
            result.RenderOutput = absolutePath(launchDirectory, *value);
        for (const auto& [name, destination, maximum] : { std::tuple<String, uint32_t*, int64_t>{ "--width", &result.Width, 8192 },
                                                          { "--height", &result.Height, 8192 },
                                                          { "--frames", &result.Frames, 1000000 } })
        {
            if (!args.HasOption(name))
                continue;
            auto value = args.GetInteger(name);
            if (!value || *value < 1 || *value > maximum)
            {
                error = name + " must be an integer between 1 and " + std::to_string(maximum);
                return false;
            }
            *destination = static_cast<uint32_t>(*value);
        }
        result.Play = args.HasOption("--play");
        result.Quit = args.HasOption("--quit");
        result.SceneCamera = args.HasOption("--scene-camera");
        if ((!result.Scene.empty() || !result.RenderOutput.empty() || result.Play) && result.Project.empty())
            error = "--scene, --render and --play require --project";
        else if ((result.Quit || result.SceneCamera || args.HasOption("--frames") || args.HasOption("--width") || args.HasOption("--height")) &&
                 result.RenderOutput.empty())
            error = "--quit, --scene-camera, --frames, --width and --height require --render";
        else if (!result.RenderOutput.empty() && result.RenderOutput.extension() != ".bmp")
            error = "--render output must have a .bmp extension";
        else if (args.HasOption("--cook-builtins") && !result.Project.empty())
            error = "--cook-builtins cannot be combined with --project";
        return error.empty();
    }
} // namespace Crowny
