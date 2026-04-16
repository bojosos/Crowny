#include "cwpch.h"

#include "Crowny/Application/Initializer.h"

// Has to be here due to ambiguous refs caused by Xlib(which is included by vulkan on linux) and Input.cpp
#include "Platform/OpenGL/OpenGLRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/StringID.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Threading/TaskSystem.h"

// Scripting
#include "Crowny/Scripting/Bindings/ScriptBindings.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{

    static TaskSystem* s_TaskSystem = nullptr;

    void Initializer::Init(const ApplicationDesc& applicationDesc)
    {
        Crowny::Log::Init(applicationDesc.Name);

        if (!ConsoleBuffer::IsStartedUp())
            ConsoleBuffer::StartUp();

        StringIDTable::StartUp();

        s_TaskSystem = new TaskSystem();

        Importer::StartUp();
        Importer::RegisterBuiltinImporters();

        AssetListenerManager::StartUp();
        AssetManager::StartUp();

        Physics2D::StartUp();
        Random::StartUp();
        AudioManager::StartUp();

        if (applicationDesc.Headless)
        {
            if (!MonoManager::IsStartedUp())
            {
                String libDir = "C:\\\\Program Files\\\\Mono\\\\lib";
                String etcDir = "C:\\\\Program Files\\\\Mono\\\\etc";
                MonoManager::StartUp(libDir, etcDir);
            }
            ScriptBindings::Register();
            ScriptInfoManager::StartUp();

            Path engineAssemblyPath = applicationDesc.EngineAssemblyPath;
            if (!engineAssemblyPath.empty())
            {
                if (engineAssemblyPath.is_relative())
                    engineAssemblyPath = applicationDesc.WorkingDirectory / engineAssemblyPath;
                if (fs::exists(engineAssemblyPath))
                {
                    MonoManager::Get().LoadAssembly(engineAssemblyPath, CROWNY_ASSEMBLY);
                    ScriptInfoManager::Get().InitializeTypes();
                    ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
                }
            }

            Path gameAssemblyPath = applicationDesc.GameAssemblyPath;
            if (!gameAssemblyPath.empty())
            {
                if (gameAssemblyPath.is_relative())
                    gameAssemblyPath = applicationDesc.WorkingDirectory / gameAssemblyPath;
                if (fs::exists(gameAssemblyPath))
                {
                    MonoManager::Get().LoadAssembly(gameAssemblyPath, GAME_ASSEMBLY);
                    ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
                }
            }

            ScriptSceneObjectManager::StartUp();
            ScriptAssetManager::StartUp();
            ScriptObjectManager::StartUp();
            return;
        }

        if (applicationDesc.PreferredAPI == RenderAPI::API::Vulkan)
            RenderAPI::StartUp<VulkanRenderAPI>();
        else if (applicationDesc.PreferredAPI == RenderAPI::API::OpenGL)
            RenderAPI::StartUp<OpenGLRenderAPI>();
        else
            CW_ENGINE_ASSERT(false, "Unknown render API");

        Renderer::Init();

        TextureDesc params;
        params.Type = TextureType::TEXTURE_DEFAULT;
        params.Shape = TextureShape::TEXTURE_2D;
        params.Usage = TextureUsage::TEXTURE_STATIC;
        params.Width = 2;
        params.Height = 2;
        params.Format = TextureFormat::RGBA8;

        Ref<PixelData> whiteData = PixelData::Create(2, 2, 1, TextureFormat::RGBA8);
        whiteData->SetColorAt(0, 0, glm::vec4(1.0f));
        whiteData->SetColorAt(0, 1, glm::vec4(1.0f));
        whiteData->SetColorAt(1, 0, glm::vec4(1.0f));
        whiteData->SetColorAt(1, 1, glm::vec4(1.0f));
        Texture::WHITE = Texture::Create(params);
        Texture::WHITE->WriteData(*whiteData);

        Ref<PixelData> blackData = PixelData::Create(2, 2, 1, TextureFormat::RGBA8);
        blackData->SetColorAt(0, 0, glm::vec4(0.0f));
        blackData->SetColorAt(0, 1, glm::vec4(0.0f));
        blackData->SetColorAt(1, 0, glm::vec4(0.0f));
        blackData->SetColorAt(1, 1, glm::vec4(0.0f));
        Texture::BLACK = Texture::Create(params);
        Texture::BLACK->WriteData(*blackData);

        gApplication->OnPreRendererInit();
        Renderer2D::Init();
        ForwardRenderer::Init();

        const Path defaultFontPath = applicationDesc.WorkingDirectory / "Crowny-Editor/Resources/Fonts/Roboto/roboto-thin.ttf.asset";
        if (fs::exists(defaultFontPath))
        {
            const AssetHandle<Font> defaultFont = gAssetManager->Load<Font>(defaultFontPath);
            if (defaultFont)
                Font::SetDefaultFont(defaultFont);
            else
                CW_ENGINE_ERROR("Default font cache not found... Regenerating...");
        }
        else
        {
            const Path rawFontPath = applicationDesc.WorkingDirectory / "Crowny-Editor/Resources/Fonts/Roboto/roboto-thin.ttf";
            if (fs::exists(rawFontPath))
            {
                const Ref<FontImportOptions> fontImportOptions = CreateRef<FontImportOptions>();
                fontImportOptions->AutomaticFontSampling = true;
                fontImportOptions->AutoSizeAtlas = true;
                const Ref<Asset> importedDefaultFont = Importer::Get().Import(rawFontPath, fontImportOptions);
                if (importedDefaultFont)
                {
                    // Save the font cache for next time.
                    gAssetManager->Save(importedDefaultFont, defaultFontPath);
                    const AssetHandle<Font> fontHandle = static_asset_cast<Font>(gAssetManager->CreateAssetHandle(importedDefaultFont));
                    Font::SetDefaultFont(fontHandle);
                }
                else
                    CW_ENGINE_ERROR("Default font not found...");
            }
        }
        // Scripting
        {
            if (!MonoManager::IsStartedUp())
            {
                String libDir = "C:\\\\Program Files\\\\Mono\\\\lib";
                String etcDir = "C:\\\\Program Files\\\\Mono\\\\etc";
                MonoManager::StartUp(libDir, etcDir);
            }
        }
        ScriptBindings::Register();
        ScriptInfoManager::StartUp();

        Path engineAssemblyPath = applicationDesc.EngineAssemblyPath;
        if (engineAssemblyPath.is_relative())
            engineAssemblyPath = applicationDesc.WorkingDirectory / engineAssemblyPath;

        if (fs::exists(engineAssemblyPath))
        {
            MonoManager::Get().LoadAssembly(engineAssemblyPath, CROWNY_ASSEMBLY);
            ScriptInfoManager::Get().InitializeTypes();
            ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
            CW_ENGINE_INFO("Loaded engine assembly {0}", engineAssemblyPath.string());
        }

        Path gameAssemblyPath = applicationDesc.GameAssemblyPath;
        if (gameAssemblyPath.is_relative())
            gameAssemblyPath = applicationDesc.WorkingDirectory / gameAssemblyPath;

        if (fs::exists(gameAssemblyPath))
        {
            MonoManager::Get().LoadAssembly(gameAssemblyPath, GAME_ASSEMBLY);
            ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
            CW_ENGINE_INFO("Loaded game assembly {0}", gameAssemblyPath.string());
        }
        ScriptSceneObjectManager::StartUp();
        ScriptAssetManager::StartUp();
        ScriptObjectManager::StartUp();

        SceneManager::StartUp();
    }

    void Initializer::Shutdown()
    {
        delete s_TaskSystem;
        s_TaskSystem = nullptr;

        Physics2D::Shutdown();
        Texture::WHITE = Texture::BLACK = nullptr;

        if (ScriptSceneObjectManager::IsStartedUp())
        {
            ScriptSceneObjectManager::Get().Del();
            ScriptSceneObjectManager::Shutdown();
        }

        ScriptObjectManager::Shutdown();
        ScriptAssetManager::Shutdown();
        ScriptInfoManager::Shutdown();
        MonoManager::Shutdown();

        if (RenderAPI::IsStartedUp())
        {
            Font::SetDefaultFont({});
            SamplerState::s_DefaultSamplerState = nullptr;

            Renderer2D::Shutdown();
            ForwardRenderer::Shutdown();
        }

        if (SceneManager::IsStartedUp())
            SceneManager::Shutdown();
        StringIDTable::Shutdown();
        VirtualFileSystem::Shutdown();
        AssetManager::Shutdown();
        AssetListenerManager::Shutdown();
        Importer::Shutdown();
        Random::Shutdown();
        AudioManager::Shutdown();

        RenderAPI::Shutdown();

        ConsoleBuffer::Shutdown();
    }

} // namespace Crowny
