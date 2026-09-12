#include "cwepch.h"

#include "Editor/EditorBuiltInAssetCompiler.h"
#include "EditorLayer.h"

#include <Crowny/Application/CmdArgs.h>
#include <Crowny/Application/EntryPoint.h>
#include <Crowny/Renderer/ForwardRenderer.h>
#include <Crowny/Utils/BuiltInShaderCompiler.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>

namespace Crowny
{
    namespace
    {
        Path FindRepositoryRoot(Path candidate)
        {
            for (uint32_t depth = 0; depth < 8 && !candidate.empty(); depth++)
            {
                if (fs::is_directory(candidate / "Crowny-Editor/Resources") && fs::is_directory(candidate / "Crowny/Source"))
                    return candidate;
                candidate = candidate.parent_path();
            }
            return {};
        }

        std::optional<RenderAPI::API> ParseRenderAPI(const CommandLineArguments& args)
        {
            for (const auto& option : args.GetOptions())
            {
                String value;
                if (option.Name == "--opengl")
                    value = "opengl";
                else if (option.Name == "--vulkan")
                    value = "vulkan";
                else if (option.Name == "--render-api")
                    value = option.Value.value_or("");
                else
                    continue;

                std::transform(value.begin(), value.end(), value.begin(),
                               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                if (value == "opengl" || value == "gl")
                    return RenderAPI::API::OpenGL;
                if (value == "vulkan" || value == "vk")
                    return RenderAPI::API::Vulkan;

                std::fprintf(stderr, "Unknown render API '%s'; using Vulkan\n", value.c_str());
                return std::nullopt;
            }
            return std::nullopt;
        }
    } // namespace

    class CrownyEditor : public Application
    {
    public:
        CrownyEditor(const Crowny::ApplicationDesc& applicationDesc, EditorLaunchOptions options)
          : Application(applicationDesc), m_LaunchOptions(std::move(options))
        {
        }

        virtual void OnPreRendererInit() override
        {
#ifdef CW_DIST
            if (!CommandLineArgs::GetParsed().HasOption("--cook-builtins"))
                return;
#endif
            BuiltInShaderCompiler::CompileAll();
            EditorBuiltInAssetCompiler::CompileChangedAssets();
        }

        virtual void OnStartUp() override
        {
            Application::OnStartUp();
            if (CommandLineArgs::GetParsed().HasOption("--cook-builtins"))
            {
                ForwardRenderer::Init();
                ForwardRenderer::Shutdown();
                Exit();
                return;
            }
            PushLayer(new EditorLayer(m_LaunchOptions));
        }

    private:
        EditorLaunchOptions m_LaunchOptions;
    };

    void CreateApplication()
    {
        const Vector<String>& args = CommandLineArgs::Get();
        EditorLaunchOptions launchOptions;
        String launchError;
        if (!ParseEditorLaunchOptions(args, fs::current_path(), launchOptions, launchError))
        {
            std::fprintf(stderr, "%s\nUse --help for editor launch options.\n", launchError.c_str());
            Application::SetExitCode(1);
            return;
        }
        if (launchOptions.Help)
        {
            std::puts("Crowny Editor\n"
                      "  --project <directory>     Open a project instead of the last project\n"
                      "  --scene <path>            Open a scene, relative to the project root\n"
                      "  --play                    Enter Play after scene loading\n"
                      "  --render <output.bmp>     Capture the viewport without editor overlays\n"
                      "  --width <1..8192>         Capture width (default 1280)\n"
                      "  --height <1..8192>        Capture height (default 720)\n"
                      "  --frames <1..1000000>     Frames before capture (default 3)\n"
                      "  --scene-camera            Use the primary scene camera for capture\n"
                      "  --quit                    Exit after capture\n"
                      "  --render-api <vulkan|gl>  Select renderer; --vulkan / --opengl also work\n"
                      "  --cook-builtins           Compile built-in assets and exit\n"
                      "  --help, -h                Show this help without starting the renderer");
            return;
        }
        if (!launchOptions.Project.empty() && !fs::is_directory(launchOptions.Project / "Assets"))
            launchError = "Project must be a directory containing Assets: " + launchOptions.Project.string();
        if (!launchOptions.Scene.empty() && !fs::is_regular_file(launchOptions.Scene))
            launchError = "Scene file does not exist: " + launchOptions.Scene.string();
        if (!launchError.empty())
        {
            std::fprintf(stderr, "%s\n", launchError.c_str());
            Application::SetExitCode(1);
            return;
        }
        const Path executableDirectory = args.empty() ? fs::current_path() : fs::absolute(args.front()).parent_path();
        Path workingDirectory = FindRepositoryRoot(fs::current_path());
        if (workingDirectory.empty())
            workingDirectory = FindRepositoryRoot(executableDirectory);
        if (workingDirectory.empty())
            workingDirectory = executableDirectory;

        Path builtInPackPath = workingDirectory / "Crowny-Editor/Resources/Builtin.cwpack";
        if (!fs::is_regular_file(builtInPackPath))
            builtInPackPath = executableDirectory / "Resources/Builtin.cwpack";

        ApplicationDesc applicationDesc;
        applicationDesc.Name = "Crowny Editor";
        applicationDesc.Window.Title = "Crowny Editor";
        applicationDesc.Window.StartMaximized = true;
        applicationDesc.Window.HideUntilSwap = true;
        if (const std::optional<RenderAPI::API> renderAPI = ParseRenderAPI(CommandLineArgs::GetParsed()))
            applicationDesc.PreferredAPI = *renderAPI;
#ifdef CW_DEBUG
        applicationDesc.Script.EnableDebugging = true;
        applicationDesc.Script.EnableProfiling = true;
#endif

        applicationDesc.WorkingDirectory = workingDirectory;
        applicationDesc.InternalDirectory = workingDirectory / "Crowny-Editor/Internal";
        applicationDesc.BuiltInResourcePackPath = builtInPackPath;
        applicationDesc.DeferRuntimeServices = true;
        Path managedAssemblyRoot;
        if (const char* configuredRoot = std::getenv("CROWNY_MANAGED_ASSEMBLY_ROOT"); configuredRoot != nullptr && configuredRoot[0] != '\0')
            managedAssemblyRoot = configuredRoot;
        else
        {
#ifdef CW_DEBUG
            constexpr const char* configuration = "Debug";
#elif defined(CW_DIST)
            constexpr const char* configuration = "Dist";
#else
            constexpr const char* configuration = "Release";
#endif
            managedAssemblyRoot = workingDirectory / ".deps/generated/managed" / configuration;
        }

        const Path generatedEngineAssembly = managedAssemblyRoot / "CrownySharp.dll";
        const Path generatedGameAssembly = managedAssemblyRoot / "GameAssembly.dll";
        applicationDesc.EngineAssemblyPath =
          fs::is_regular_file(generatedEngineAssembly) ? generatedEngineAssembly : workingDirectory / "Crowny-Sharp/CrownySharp.dll";
        applicationDesc.GameAssemblyPath =
          fs::is_regular_file(generatedGameAssembly) ? generatedGameAssembly : workingDirectory / "Crowny-Sandbox/GameAssembly.dll";

        Application::StartUp<CrownyEditor>(applicationDesc, std::move(launchOptions));
    }
} // namespace Crowny
