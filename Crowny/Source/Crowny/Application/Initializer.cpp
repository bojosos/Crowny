#include "cwpch.h"

#include "Crowny/Application/Initializer.h"

// Has to be here due to ambiguous refs caused by Xlib(which is included by vulkan on linux) and my Input class
#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/Random.h"
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

// Script runtime
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

// Importers
#include "Crowny/Import/AudioClipImporter.h"
#include "Crowny/Import/FontImporter.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Import/ScriptImporter.h"
#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Import/TextFileImporter.h"
#include "Crowny/Import/TextureImporter.h"

#include "Crowny/Common/ConsoleBuffer.h"

namespace Crowny
{

    void Initializer::Init(const ApplicationDesc& applicationDesc)
    {
        Crowny::Log::Init(applicationDesc.Name);

        ConsoleBuffer::StartUp();

        Importer::StartUp();
        Importer::Get().RegisterImporter(new AudioClipImporter());
        Importer::Get().RegisterImporter(new FontImporter());
        Importer::Get().RegisterImporter(new ScriptImporter());
        Importer::Get().RegisterImporter(new ShaderImporter());
        Importer::Get().RegisterImporter(new TextFileImporter());
        Importer::Get().RegisterImporter(new TextureImporter());
        Importer::Get().RegisterImporter(new MeshImporter());

        AssetManager::StartUp();
        // Most of these should be in the editor
        VirtualFileSystem::Init();
        VirtualFileSystem::Get()->Mount("Shaders", "Resources/Shaders");
        VirtualFileSystem::Get()->Mount("Textures", "Resources/Textures");
        VirtualFileSystem::Get()->Mount("Fonts", "Resources/Fonts");
        VirtualFileSystem::Get()->Mount("Assemblies", "Resources/Assemblies");
        VirtualFileSystem::Get()->Mount("Models", "Resources/Models");
        VirtualFileSystem::Get()->Mount("Cache", "Resources/Cache");

        Physics2D::StartUp();
        Random::StartUp();
        AudioManager::StartUp();
        RenderAPI::StartUp<VulkanRenderAPI>();
        Renderer::Init();

        TextureParameters params;
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

        Renderer2D::Init();
        ForwardRenderer::Init();

        // FontManager::Add(CreateRef<Font>(Path("Resources/Fonts/Roboto") / DEFAULT_FONT_FILENAME, "Roboto
        // Thin", 64.0f));

        // Scripting
        MonoManager::StartUp();
        Path engineAssemblyPath = Path("C:/dev/Crowny/Crowny-Sharp") / (std::string(CROWNY_ASSEMBLY) + ".dll");
        if (fs::exists(engineAssemblyPath))
        {
            MonoManager::Get().LoadAssembly(engineAssemblyPath, CROWNY_ASSEMBLY);
            ScriptInfoManager::StartUp();
            ScriptInfoManager::Get().InitializeTypes();
            ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
            CW_ENGINE_INFO("Loaded engine assembly {0}", engineAssemblyPath.string());
        }

        Path gameAssmeblyPath =
          Path("C:/dev/Projects/Project1/Internal/Assemblies/Debug/") / (std::string(GAME_ASSEMBLY) + ".dll");
        if (fs::exists(gameAssmeblyPath))
        {
            MonoManager::Get().LoadAssembly(gameAssmeblyPath, GAME_ASSEMBLY);
            ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
            CW_ENGINE_INFO("Loaded game assembly {0}", gameAssmeblyPath.string());
        }
        ScriptSceneObjectManager::StartUp();
        ScriptAssetManager::StartUp();
        ScriptObjectManager::StartUp();
    }

    void Initializer::Shutdown()
    {
        Physics2D::Shutdown();
        Texture::WHITE = Texture::BLACK = nullptr;
        ScriptSceneObjectManager::Get().Del();
        ScriptRuntime::UnloadAssemblies();
        Renderer2D::Shutdown();
        SamplerState::s_DefaultSamplerState = nullptr;
        /*
        Renderer::Shutdown();
        ForwardPlusRenderer::Shutdown();
        */
        SceneManager::Shutdown();
        VirtualFileSystem::Shutdown();
        AssetManager::Shutdown();
        Importer::Shutdown();
        AudioManager::Shutdown();
        // TODO: reenable ForwardRenderer::Shutdown();
        RenderAPI::Get().Shutdown();

        ScriptInfoManager::Shutdown();
        // ScriptSceneObjectManager::Shutdown();
        // ScriptSceneObjectManager::Shutdown();
        // ScriptObjectManager::Shutdown();
    }

} // namespace Crowny
