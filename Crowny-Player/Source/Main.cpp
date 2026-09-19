#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Build/GamePackage.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Input/InputMapSerialization.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace Crowny
{
    class PlayerLayer final : public Layer
    {
    public:
        explicit PlayerLayer(uint32_t frames) : m_Frames(frames) {}

        void OnAttach() override
        {
            m_Renderer = CreateScope<SceneRenderer>(SceneManager::Get().GetActiveScene(), nullptr);
            m_Renderer->Init();
            Application::Get().GetImGuiLayer()->BlockEvents(false);
            ImGui::GetIO().IniFilename = nullptr;
        }

        void OnUpdate(Timestep) override
        {
            Application& app = Application::Get();
            SceneManager& scenes = SceneManager::Get();
            scenes.ProcessDeferredOperations();
            Ref<Scene> scene = scenes.GetActiveScene();
            if (!scene)
                throw std::runtime_error("The game has no active scene.");
            m_Renderer->SetScene(scene);
            const auto& window = app.GetRenderWindow()->GetProperties();
            if (window.Width == 0 || window.Height == 0)
                return;
            if (!m_Target || m_Target->GetProperties().Width != window.Width || m_Target->GetProperties().Height != window.Height)
            {
                if (auto* thread = app.GetRenderThread())
                    thread->WaitForFrameDone();
                if (m_Target)
                    ImGuiVulkanTexture::Release(m_Target->GetColorTexture(0));
                TextureDesc color;
                color.Width = window.Width;
                color.Height = window.Height;
                color.Usage = TextureUsage::TEXTURE_RENDERTARGET;
                TextureDesc depth = color;
                depth.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
                depth.Format = TextureFormat::DEPTH24STENCIL8;
                RenderTextureDesc target;
                target.Width = window.Width;
                target.Height = window.Height;
                target.ColorSurfaces[0].Texture = Texture::Create(color);
                target.DepthSurface.Texture = Texture::Create(depth);
                m_Target = RenderTexture::Create(target);
                m_Renderer->SetRenderTarget(m_Target);
            }
            scene->OnViewportResize(window.Width, window.Height);
            Time& time = app.GetTime();
            const SimulationFrame frame = time.AdvanceSimulation(*app.GetTimeSettings());
            const Timestep delta(time.GetDeltaTime());
            for (uint32_t step = 0; step < frame.FixedStepCount; ++step)
            {
                ScriptRuntime::OnFixedUpdate(scene, frame.FixedDelta);
                scene->OnFixedUpdate(frame.FixedDelta);
            }
            ScriptRuntime::OnUpdate(delta, false);
            scene->OnUpdateRuntime(delta);
            m_Renderer->UpdateAnimations(delta);
            ScriptRuntime::OnLateUpdate(delta);
            m_Renderer->UpdateProceduralMeshes();
            if (auto* thread = app.GetRenderThread(); thread && thread->IsRunning())
            {
                auto& snapshot = thread->BeginFrame();
                m_Renderer->ExtractSnapshot(snapshot);
                thread->SubmitFrame();
                thread->WaitForFrameDone();
            }
            else
            {
                m_Snapshot.Clear();
                m_Renderer->ExtractSnapshot(m_Snapshot);
                SceneRenderer::RenderFromSnapshot(m_Snapshot);
            }
            if (m_Frames && ++m_FrameNumber >= m_Frames)
                app.Exit();
        }

        void OnImGuiRender() override
        {
            if (!m_Target)
                return;
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::GetBackgroundDrawList()->AddImage(ImGuiVulkanTexture::Get(m_Target->GetColorTexture(0)), viewport->Pos,
                                                     ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), ImVec2(0, 1),
                                                     ImVec2(1, 0));
        }

    private:
        Scope<SceneRenderer> m_Renderer;
        Ref<RenderTexture> m_Target;
        RenderSnapshot m_Snapshot;
        uint32_t m_Frames = 0;
        uint32_t m_FrameNumber = 0;
    };

    Path ExecutableDirectory()
    {
#ifdef CW_PLATFORM_WIN32
        wchar_t path[32768];
        const DWORD count = GetModuleFileNameW(nullptr, path, 32768);
        if (!count || count == 32768)
            throw std::runtime_error("Could not locate the player executable.");
        return Path(std::wstring(path, count)).parent_path();
#else
        return fs::canonical("/proc/self/exe").parent_path();
#endif
    }
} // namespace Crowny

int main(int argc, char** argv)
{
    using namespace Crowny;
    Path report;
    try
    {
        const Path root = ExecutableDirectory();
        uint32_t frames = 0;
        bool openGL = false;
        bool hidden = false;
        for (int index = 1; index < argc; ++index)
        {
            const String argument = argv[index];
            if (argument == "--frames" && index + 1 < argc)
                frames = static_cast<uint32_t>(std::stoul(argv[++index]));
            else if (argument == "--report" && index + 1 < argc)
                report = fs::absolute(argv[++index]);
            else if (argument == "--opengl")
                openGL = true;
            else if (argument == "--hidden")
                hidden = true;
            else
                throw std::runtime_error("Unknown player argument: " + argument);
        }
        GamePackage package;
        if (const String error = package.Open(root); !error.empty())
            throw std::runtime_error(error);
        const auto& manifest = package.GetManifest();
        fs::current_path(root);
        ApplicationDesc description;
        description.Name = manifest.ProductName;
        description.Window.Title = manifest.ProductName;
        description.Window.Hidden = hidden;
        description.WorkingDirectory = root;
        description.BuiltInResourcePackPath = "Resources/Builtin.cwpack";
        description.EngineAssemblyPath = "Managed/CrownySharp.dll";
        description.GameAssemblyPath = manifest.Paths.ManagedAssembly;
        description.Script.RuntimeRoot = root / "Mono";
        description.PreferredAPI = openGL || manifest.Renderers == RendererPolicy::OpenGLOnly ? RenderAPI::API::OpenGL : RenderAPI::API::Vulkan;
        Application::StartUp(description);
        AssetManager::Get().RegisterAssetManifest(package.GetAssets());
        if (!package.GetSettingsPath().empty())
            Input::SetActionMap(DeserializeInputMap(YAML::LoadFile(package.GetSettingsPath().string())["Input"]));
        if (SceneManager::Get().LoadScene(manifest.StartupScene) != SceneOperationStatus::Completed)
            throw std::runtime_error("Could not load the startup scene.");
        if (!SceneManager::Get().GetActiveScene()->GetPrimaryCameraEntity())
            throw std::runtime_error("The startup scene needs a Camera component to render the game.");
        if (SceneManager::Get().BeginPlay() != SceneOperationStatus::Completed)
            throw std::runtime_error("Could not start the game scene.");
        Application::Get().PushLayer(new PlayerLayer(frames));
        Application::Get().Run();
        if (auto* thread = Application::Get().GetRenderThread())
            thread->WaitForFrameDone();
        const auto statistics = SceneRenderer::GetStatistics();
        SceneManager::Get().Stop();
        Application::Shutdown();
        if (frames && !statistics.FrameRendered)
            throw std::runtime_error("The player did not render a successful frame.");
        if (!report.empty())
            std::ofstream(report) << "Player completed successfully. Scene: " << manifest.StartupScene.ToString()
                                  << "\nVisible instances: " << statistics.VisibleInstances << "\nVisible sprites: " << statistics.VisibleSprites2D
                                  << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        if (!report.empty())
            std::ofstream(report) << "Player failed: " << error.what() << '\n';
        std::cerr << error.what() << '\n';
        if (Application::IsStartedUp())
            Application::Shutdown();
#ifdef CW_PLATFORM_WIN32
        if (report.empty())
            MessageBoxA(nullptr, error.what(), "Unable to start game", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
}
