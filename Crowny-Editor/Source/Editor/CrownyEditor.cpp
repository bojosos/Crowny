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

        std::optional<RenderAPI::API> ParseRenderAPI(const Vector<String>& args)
        {
            for (size_t index = 1; index < args.size(); ++index)
            {
                String value;
                if (args[index] == "--opengl")
                    value = "opengl";
                else if (args[index] == "--vulkan")
                    value = "vulkan";
                else if (args[index] == "--render-api" && index + 1 < args.size())
                    value = args[++index];
                else if (args[index].starts_with("--render-api="))
                    value = args[index].substr(String("--render-api=").size());
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
        CrownyEditor(const Crowny::ApplicationDesc& applicationDesc) : Application(applicationDesc) {}

        virtual void OnPreRendererInit() override
        {
#ifdef CW_DIST
            const Vector<String>& args = CommandLineArgs::Get();
            if (std::find(args.begin(), args.end(), "--cook-builtins") == args.end())
                return;
#endif
            BuiltInShaderCompiler::CompileAll();
            EditorBuiltInAssetCompiler::CompileChangedAssets();
        }

        virtual void OnStartUp() override
        {
            Application::OnStartUp();
            const Vector<String>& args = CommandLineArgs::Get();
            if (std::find(args.begin(), args.end(), "--cook-builtins") != args.end())
            {
                ForwardRenderer::Init();
                ForwardRenderer::Shutdown();
                Exit();
                return;
            }
            PushLayer(new EditorLayer());
        }
    };

    void CreateApplication()
    {
        const Vector<String>& args = CommandLineArgs::Get();
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
        if (const std::optional<RenderAPI::API> renderAPI = ParseRenderAPI(args))
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

        Application::StartUp<CrownyEditor>(applicationDesc);
    }
} // namespace Crowny
