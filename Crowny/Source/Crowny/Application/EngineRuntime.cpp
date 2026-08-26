#include "cwpch.h"

#include "Crowny/Application/EngineRuntime.h"

// Has to be here due to ambiguous refs caused by Xlib(which is included by vulkan on linux) and Input.cpp
#include "Platform/OpenGL/OpenGLRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
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
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Threading/TaskSystem.h"

// Scripting
#include "Crowny/Scripting/Bindings/ScriptBindings.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

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

        bool StartMono(const ApplicationDesc& applicationDesc)
        {
            if (MonoManager::IsStartedUp())
                return false;

            const MonoRuntimePaths monoPaths = ResolveMonoRuntimePaths(applicationDesc.WorkingDirectory);
            if (!monoPaths.HasRuntime())
            {
                CW_ENGINE_ERROR("Mono runtime not found. Set CROWNY_MONO_ROOT or MONO_SDK, or run Scripts/setup-windows.ps1.");
                return false;
            }

            CW_ENGINE_INFO("Using Mono runtime at {0}", monoPaths.Root.string());
            const uint32_t debugPort = applicationDesc.Script.EnableDebugging ? 17615 : 0;
            MonoManager::StartUp(monoPaths.LibraryDirectory, monoPaths.EtcDirectory, debugPort);
            return true;
        }

        void LoadDefaultFont()
        {
            if (!RenderAPI::IsStartedUp() || Font::GetDefaultFont())
                return;

            const Path defaultFontPath = "Resources/Fonts/Roboto/roboto-thin.ttf.asset";
            if (FileSystem::FileExists(defaultFontPath))
            {
                const AssetHandle<Font> defaultFont = AssetManager::Get().Load<Font>(defaultFontPath);
                if (defaultFont)
                    Font::SetDefaultFont(defaultFont);
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

        // Initialize the embedded runtime before Crowny creates its worker pool so
        // Mono's process-wide thread and GC bookkeeping is established first.
        // Managed assemblies and higher-level scripting services remain deferred.
        if (!MonoManager::IsStartedUp())
        {
            m_State->ServiceShutdownActions.emplace_back([]() { MonoManager::Shutdown(); });
            if (!StartMono(m_State->Description))
                m_State->ServiceShutdownActions.pop_back();
        }

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

        if (ScriptObjectManager::IsStartedUp())
            return;

        if (!MonoManager::IsStartedUp())
            return;

        ScriptBindings::Register();
        StartOwnedModule<ScriptInfoManager>(m_State->ServiceShutdownActions);

        Path engineAssemblyPath = m_State->Description.EngineAssemblyPath;
        if (!engineAssemblyPath.empty())
        {
            if (engineAssemblyPath.is_relative())
                engineAssemblyPath = m_State->Description.WorkingDirectory / engineAssemblyPath;
            if (fs::exists(engineAssemblyPath))
            {
                MonoManager::Get().LoadAssembly(engineAssemblyPath, CROWNY_ASSEMBLY);
                ScriptInfoManager::Get().InitializeTypes();
                ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
                CW_ENGINE_INFO("Loaded engine assembly {0}", engineAssemblyPath.string());
            }
        }

        Path gameAssemblyPath = m_State->Description.GameAssemblyPath;
        if (!gameAssemblyPath.empty())
        {
            if (gameAssemblyPath.is_relative())
                gameAssemblyPath = m_State->Description.WorkingDirectory / gameAssemblyPath;
            if (fs::exists(gameAssemblyPath))
            {
                MonoManager::Get().LoadAssembly(gameAssemblyPath, GAME_ASSEMBLY);
                ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
                CW_ENGINE_INFO("Loaded game assembly {0}", gameAssemblyPath.string());
            }
        }

        if (!ScriptSceneObjectManager::IsStartedUp())
        {
            m_State->ServiceShutdownActions.emplace_back([]() {
                if (!ScriptSceneObjectManager::IsStartedUp())
                    return;
                ScriptSceneObjectManager::Get().Del();
                ScriptSceneObjectManager::Shutdown();
            });
            try
            {
                ScriptSceneObjectManager::StartUp();
            }
            catch (...)
            {
                m_State->ServiceShutdownActions.pop_back();
                throw;
            }
        }
        StartOwnedModule<ScriptObjectManager>(m_State->ServiceShutdownActions);
        StartOwnedModule<ScriptAssetManager>(m_State->ServiceShutdownActions);
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

        Font::SetDefaultFont({});
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
} // namespace Crowny
