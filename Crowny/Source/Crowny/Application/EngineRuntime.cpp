#include "cwpch.h"

#include "Crowny/Application/EngineRuntime.h"

// Has to be here due to ambiguous refs caused by Xlib(which is included by vulkan on linux) and Input.cpp
#include "Platform/OpenGL/OpenGLRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/PrimitiveMeshLibrary.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/BuiltInResourcePack.h"
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/StringID.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/Renderer/FontManager.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Threading/TaskSystem.h"

#include "Crowny/Scripting/Managed/ManagedBackendSelection.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/ManagedReload.h"

namespace Crowny
{
    namespace
    {
        using ShutdownAction = std::function<void()>;

        template <class T, class... Args> bool StartOwnedModule(Vector<ShutdownAction>& shutdownActions, Args&&... args)
        {
            if (T::IsStartedUp())
                return false;

            shutdownActions.emplace_back([]() { T::Shutdown(); });
            try
            {
                T::StartUp(std::forward<Args>(args)...);
            }
            catch (...)
            {
                shutdownActions.pop_back();
                throw;
            }
            return true;
        }

        template <class T, class SubType> bool StartOwnedModule(Vector<ShutdownAction>& shutdownActions)
        {
            if (T::IsStartedUp())
                return false;

            shutdownActions.emplace_back([]() { T::Shutdown(); });
            try
            {
                T::template StartUp<SubType>();
            }
            catch (...)
            {
                shutdownActions.pop_back();
                throw;
            }
            return true;
        }

        void RunShutdownActions(Vector<ShutdownAction>& shutdownActions)
        {
            while (!shutdownActions.empty())
            {
                ShutdownAction shutdown = std::move(shutdownActions.back());
                shutdownActions.pop_back();
                shutdown();
            }
        }

        void LogManagedDiagnostics(const ManagedOperationResult& result)
        {
            for (const ManagedDiagnostic& diagnostic : result.Diagnostics)
            {
                if (diagnostic.Severity == ManagedDiagnosticSeverity::Error)
                    CW_ENGINE_ERROR("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                else if (diagnostic.Severity == ManagedDiagnosticSeverity::Warning)
                    CW_ENGINE_WARN("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                else
                    CW_ENGINE_INFO("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
            }
        }

        void LoadDefaultFont()
        {
            if (!RenderAPI::IsStartedUp() || FontManager::GetDefaultFont())
                return;

            const Path defaultFontPath = "Resources/Fonts/Roboto/roboto-thin.ttf.asset";
            if (FileSystem::FileExists(defaultFontPath))
            {
                const AssetHandle<Font> defaultFont = AssetManager::Get().Load<Font>(defaultFontPath);
                if (defaultFont)
                    FontManager::SetDefaultFont(defaultFont);
                else
                    CW_ENGINE_ERROR("Default font cache could not be loaded.");
                return;
            }

            CW_ENGINE_ERROR("Default font asset is missing from the built-in resource pack.");
        }
    } // namespace

    struct EngineRuntime::State
    {
        explicit State(const ApplicationDesc& applicationDesc) : Description(applicationDesc)
        {
            FinalShutdownActions.reserve(2);
            RenderAPIShutdownActions.reserve(1);
            CoreShutdownActions.reserve(8);
            ServiceShutdownActions.reserve(10);
        }

        void DrainTaskSystem()
        {
            if (TaskSystemDrained)
                return;

            TaskSystemDrained = true;
            if (OwnsTaskSystem && TaskSystem::IsStartedUp())
                TaskSystem::Get().Drain();
        }

        ApplicationDesc Description;
        Vector<ShutdownAction> FinalShutdownActions;
        Vector<ShutdownAction> RenderAPIShutdownActions;
        Vector<ShutdownAction> CoreShutdownActions;
        Vector<ShutdownAction> ServiceShutdownActions;
        bool Started = false;
        bool RendererStarted = false;
        bool RendererResourcesStarted = false;
        bool OwnsTaskSystem = false;
        bool OwnsSceneManager = false;
        bool TaskSystemDrained = false;
        Scope<ManagedScripting> Managed;
        ManagedProgramDefinition ManagedProgram;
        bool HasManagedProgram = false;
        bool ManagedProgramLoaded = false;
    };

    EngineRuntime::EngineRuntime(const ApplicationDesc& applicationDesc) : m_State(CreateScope<State>(applicationDesc)) {}

    EngineRuntime::~EngineRuntime()
    {
        StopRenderer();
        ShutdownRendererResources();
        ShutdownServices();
        ShutdownCoreServices();
        ShutdownRenderAPI();
    }

    void EngineRuntime::Start()
    {
        if (m_State->Started)
            return;

        StartOwnedModule<ConsoleBuffer>(m_State->FinalShutdownActions);
        Crowny::Log::Init(m_State->Description.Name);

        if (!m_State->Description.BuiltInResourcePackPath.empty())
        {
            Path packPath = m_State->Description.BuiltInResourcePackPath;
            if (packPath.is_relative())
                packPath = m_State->Description.WorkingDirectory / packPath;
            StartOwnedModule<BuiltInResourcePack>(m_State->FinalShutdownActions, packPath);
            if (BuiltInResourcePack::Get().IsValid())
                CW_ENGINE_INFO("Loaded {} packed built-in resources from {}", BuiltInResourcePack::Get().GetEntryCount(), packPath.string());
            else
            {
                CW_ENGINE_WARN("Built-in resource pack not found or invalid at {}. Using loose resources.", packPath.string());
                BuiltInResourcePack::Shutdown();
            }
        }

        StartOwnedModule<StringIDTable>(m_State->CoreShutdownActions);

        // Initialize the selected runtime before Crowny creates its worker pool.
        // Managed assemblies and higher-level scripting services remain deferred.
        ManagedBackendSelection selection = ResolveManagedBackendPreset(m_State->Description.Script.Backend);
        selection.Runtime.EnableDebugging = m_State->Description.Script.EnableDebugging;
        selection.Runtime.EnableProfiling = m_State->Description.Script.EnableProfiling;
        selection.Runtime.RuntimeRoot = m_State->Description.Script.RuntimeRoot;

        if (selection.Runtime.Backend == ManagedBackendId::Mono && selection.Runtime.RuntimeRoot.empty())
        {
            const MonoRuntimePaths monoPaths = ResolveMonoRuntimePaths(m_State->Description.WorkingDirectory);
            if (monoPaths.HasRuntime())
            {
                selection.Runtime.RuntimeRoot = monoPaths.Root;
                CW_ENGINE_INFO("Using Mono runtime at {}", monoPaths.Root.string());
            }
        }

        if (selection.Runtime.Backend == ManagedBackendId::CoreCLR && !m_State->Description.Script.ProgramManifest.empty())
        {
            Path manifest = m_State->Description.Script.ProgramManifest;
            if (manifest.is_relative())
                manifest = m_State->Description.WorkingDirectory / manifest;
            ManagedProgramPackageResult package = LoadManagedProgramPackage(manifest);
            if (package.Result.Succeeded)
            {
                package.Package.Runtime.EnableDebugging = selection.Runtime.EnableDebugging;
                package.Package.Runtime.EnableProfiling = selection.Runtime.EnableProfiling;
                selection.Runtime = std::move(package.Package.Runtime);
                m_State->ManagedProgram = std::move(package.Package.Program);
                m_State->HasManagedProgram = true;
            }
            else
                LogManagedDiagnostics(package.Result);
        }

        Scope<ManagedScripting> managed = CreateScope<ManagedScripting>();
        ManagedOperationResult managedStarted = managed->Start(selection.Runtime);
        if (managedStarted.Succeeded)
        {
            m_State->Managed = std::move(managed);
            ManagedScripting* managedRuntime = m_State->Managed.get();
            m_State->ServiceShutdownActions.emplace_back([managedRuntime]() { managedRuntime->Shutdown(); });
        }
        else
            LogManagedDiagnostics(managedStarted);

        m_State->OwnsTaskSystem = StartOwnedModule<TaskSystem>(m_State->CoreShutdownActions);
        StartOwnedModule<Importer>(m_State->CoreShutdownActions);
        Importer::RegisterBuiltinImporters();
        StartOwnedModule<AssetListenerManager>(m_State->CoreShutdownActions);
        StartOwnedModule<AssetManager>(m_State->CoreShutdownActions);
        StartOwnedModule<Random>(m_State->CoreShutdownActions);

        if (!m_State->Description.Headless)
        {
            if (m_State->Description.PreferredAPI == RenderAPI::API::Vulkan)
                StartOwnedModule<RenderAPI, VulkanRenderAPI>(m_State->RenderAPIShutdownActions);
            else if (m_State->Description.PreferredAPI == RenderAPI::API::OpenGL)
                StartOwnedModule<RenderAPI, OpenGLRenderAPI>(m_State->RenderAPIShutdownActions);
            else
                CW_ENGINE_ASSERT(false, "Unknown render API");

            // Vulkan creates its instance and devices before the window surface. OpenGL
            // needs a current window context first, so initialization waits for StartRenderer().
            if (m_State->Description.PreferredAPI == RenderAPI::API::Vulkan)
            {
                Renderer::Init();
                m_State->RendererStarted = true;
            }
        }

        m_State->Started = true;
        if (m_State->Description.Headless)
            StartRuntimeServices();
    }

    void EngineRuntime::StartRenderer()
    {
        CW_ENGINE_ASSERT(!m_State->Description.Headless, "A headless application cannot initialize a renderer");
        CW_ENGINE_ASSERT(RenderAPI::IsStartedUp(), "Render API must be started before renderer resources");

        if (m_State->RendererResourcesStarted)
            return;

        if (!m_State->RendererStarted)
        {
            Renderer::Init();
            m_State->RendererStarted = true;
        }

        TextureDesc params;
        params.Type = TextureType::TEXTURE_DEFAULT;
        params.Shape = TextureShape::TEXTURE_2D;
        params.Usage = TextureUsage::TEXTURE_STATIC;
        params.Width = 1;
        params.Height = 1;
        params.Format = TextureFormat::RGBA8;

        const auto createSolidTexture = [&params](StringView debugName, const glm::vec4& color) {
            params.DebugName = debugName;
            Ref<PixelData> data = PixelData::Create(1, 1, 1, TextureFormat::RGBA8);
            data->SetColorAt(0, 0, color);
            Ref<Texture> texture = Texture::Create(params);
            texture->WriteData(*data);
            return texture;
        };

        Texture::WHITE = createSolidTexture("BuiltIn/White", glm::vec4(1.0f));
        Texture::BLACK = createSolidTexture("BuiltIn/Black", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        Texture::NORMAL = createSolidTexture("BuiltIn/Normal", glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));

        params.DebugName = "BuiltIn/Error";
        params.Width = 2;
        params.Height = 2;
        Ref<PixelData> errorData = PixelData::Create(2, 2, 1, TextureFormat::RGBA8);
        const glm::vec4 magenta(1.0f, 0.0f, 1.0f, 1.0f);
        const glm::vec4 opaqueBlack(0.0f, 0.0f, 0.0f, 1.0f);
        errorData->SetColorAt(0, 0, magenta);
        errorData->SetColorAt(1, 0, opaqueBlack);
        errorData->SetColorAt(0, 1, opaqueBlack);
        errorData->SetColorAt(1, 1, magenta);
        Texture::MISSING = Texture::Create(params);
        Texture::MISSING->WriteData(*errorData);

        Application::Get().OnPreRendererInit();
        if (!m_State->Description.DeferRuntimeServices)
        {
            Renderer2D::Init();
            ForwardRenderer::Init();
        }
        m_State->OwnsSceneManager = StartOwnedModule<SceneManager>(m_State->ServiceShutdownActions);
        // Built-in primitive meshes (Cube, Sphere, ...) live under fixed UUIDs so scenes can reference them
        // without a project asset; they need both the AssetManager and a started RenderAPI.
        PrimitiveMeshLibrary::EnsureRegistered();
        m_State->ServiceShutdownActions.emplace_back([]() { PrimitiveMeshLibrary::Shutdown(); });
        m_State->RendererResourcesStarted = true;

        if (!m_State->Description.DeferRuntimeServices)
            StartRuntimeServices();
    }

    void EngineRuntime::StartRuntimeServices()
    {
        StartOwnedModule<Physics2D>(m_State->ServiceShutdownActions);
        StartOwnedModule<Physics3D>(m_State->ServiceShutdownActions);
        StartOwnedModule<AudioManager>(m_State->ServiceShutdownActions);
        LoadDefaultFont();

        if (m_State->Managed == nullptr || !m_State->Managed->IsStarted() || m_State->ManagedProgramLoaded)
            return;

        if (!m_State->HasManagedProgram && m_State->Description.Script.Backend == ManagedBackendPreset::Mono)
        {
            ManagedProgramDefinition program;
            program.Generation = 1;
            Path engineAssemblyPath = m_State->Description.EngineAssemblyPath;
            if (engineAssemblyPath.is_relative())
                engineAssemblyPath = m_State->Description.WorkingDirectory / engineAssemblyPath;
            if (m_State->Description.EngineAssemblyPath.empty())
                return;
            program.Artifacts.push_back({ ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY, std::move(engineAssemblyPath) });

            Path gameAssemblyPath = m_State->Description.GameAssemblyPath;
            if (gameAssemblyPath.is_relative())
                gameAssemblyPath = m_State->Description.WorkingDirectory / gameAssemblyPath;
            if (!m_State->Description.GameAssemblyPath.empty() && fs::is_regular_file(gameAssemblyPath))
                program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY, std::move(gameAssemblyPath) });
            m_State->ManagedProgram = std::move(program);
            m_State->HasManagedProgram = true;
        }

        if (!m_State->HasManagedProgram)
            return;
        ManagedOperationResult loaded = m_State->Managed->LoadProgram(m_State->ManagedProgram);
        if (!loaded.Succeeded)
        {
            LogManagedDiagnostics(loaded);
            return;
        }
        m_State->ManagedProgramLoaded = true;
        CW_ENGINE_INFO("Loaded managed program generation {} with {} script types using {}", m_State->ManagedProgram.Generation,
                       m_State->Managed->GetScriptCatalog().Types.size(),
                       ToString(ResolveManagedBackendPreset(m_State->Description.Script.Backend).Runtime.Backend));
    }

    void EngineRuntime::StopRenderer()
    {
        if (!m_State->RendererStarted)
            return;
        Renderer::Shutdown();
        m_State->RendererStarted = false;
    }

    void EngineRuntime::ShutdownRendererResources()
    {
        if (!m_State->RendererResourcesStarted)
            return;

        FontManager::Clear();
        SamplerState::s_DefaultSamplerState = nullptr;
        SamplerState::ClearCache();
        Renderer2D::Shutdown();
        ForwardRenderer::Shutdown();
        Texture::WHITE = Texture::BLACK = Texture::NORMAL = Texture::MISSING = nullptr;
        m_State->RendererResourcesStarted = false;
    }

    void EngineRuntime::ShutdownServices()
    {
        if (m_State->OwnsSceneManager && SceneManager::IsStartedUp())
            SceneManager::Get().Stop();
        m_State->DrainTaskSystem();
        RunShutdownActions(m_State->ServiceShutdownActions);
        m_State->OwnsSceneManager = false;
    }

    void EngineRuntime::ShutdownCoreServices()
    {
        m_State->DrainTaskSystem();
        RunShutdownActions(m_State->CoreShutdownActions);
    }

    void EngineRuntime::ShutdownRenderAPI()
    {
        RunShutdownActions(m_State->RenderAPIShutdownActions);
        RunShutdownActions(m_State->FinalShutdownActions);
    }

    ManagedScripting* EngineRuntime::GetManagedScripting() { return m_State->Managed.get(); }

    const ManagedScripting* EngineRuntime::GetManagedScripting() const { return m_State->Managed.get(); }
} // namespace Crowny
