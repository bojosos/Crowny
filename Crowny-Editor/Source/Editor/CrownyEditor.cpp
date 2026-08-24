#include "cwepch.h"

#include "EditorLayer.h"
#include "Editor/EditorBuiltInAssetCompiler.h"

#include <Crowny/Application/CmdArgs.h>
#include <Crowny/Application/EntryPoint.h>
#include <Crowny/Renderer/ForwardRenderer.h>
#include <Crowny/Utils/BuiltInShaderCompiler.h>

#include <cctype>
#include <cstdio>
#include <optional>

namespace Crowny
{
    namespace
    {
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

                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
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
#ifndef CW_DIST
            BuiltInShaderCompiler::CompileAll();
            EditorBuiltInAssetCompiler::CompileChangedAssets();
#endif
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
        Path workingDirectory;
        Path candidate = fs::current_path();
        for (uint32_t depth = 0; depth < 8 && !candidate.empty(); depth++)
        {
            if (fs::is_directory(candidate / "Crowny-Editor/Resources") && fs::is_directory(candidate / "Crowny/Source"))
            {
                workingDirectory = candidate;
                break;
            }
            candidate = candidate.parent_path();
        }
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
        applicationDesc.EngineAssemblyPath = "Crowny-Sharp/CrownySharp.dll";
        applicationDesc.GameAssemblyPath = "Crowny-Sandbox/GameAssembly.dll";

        Application::StartUp<CrownyEditor>(applicationDesc);
    }
} // namespace Crowny
