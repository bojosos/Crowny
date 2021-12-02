#include "cwpch.h"

#include "Crowny/Application/Initializer.h"

// Has to be here due to ambiguous refs caused by Xlib(which is included by vulkan on linux) and my Input class
#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"

// Script runtime
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

// Importers
#include "Crowny/Import/AudioClipImporter.h"
#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Import/TextureImporter.h"

namespace Crowny
{

    void Initializer::Init()
    {
        Importer::StartUp();
        Importer::Get().RegisterImporter(new AudioClipImporter());
        Importer::Get().RegisterImporter(new ShaderImporter());
        Importer::Get().RegisterImporter(new TextureImporter());

        AssetManager::StartUp();
        // Most of these should be in the editor
        VirtualFileSystem::Init();
        VirtualFileSystem::Get()->Mount("Shaders", "Resources/Shaders");
        VirtualFileSystem::Get()->Mount("Textures", "Resources/Textures");
        VirtualFileSystem::Get()->Mount("Fonts", "Resources/Fonts");
        VirtualFileSystem::Get()->Mount("Assemblies", "Resources/Assemblies");
        VirtualFileSystem::Get()->Mount("Models", "Resources/Models");
        VirtualFileSystem::Get()->Mount("Cache", "Resources/Cache");

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
        // ForwardRenderer::Init();

        FontManager::Add(CreateRef<Font>("Resources/Fonts/" + DEFAULT_FONT_FILENAME, "Roboto Thin", 64));

        // Scripting
        MonoManager::StartUp();
        MonoManager::Get().LoadAssembly(String("Resources/Assemblies/") + CROWNY_ASSEMBLY + ".dll", CROWNY_ASSEMBLY);
        MonoManager::Get().LoadAssembly(String("Resources/Assemblies/") + GAME_ASSEMBLY + ".dll", GAME_ASSEMBLY);
        ScriptInfoManager::StartUp();
        ScriptInfoManager::Get().InitializeTypes();
        ScriptSceneObjectManager::StartUp();
        ScriptObjectManager::StartUp();
    }

    void Initializer::Shutdown()
    {
        Renderer2D::Shutdown();
        SamplerState::s_DefaultSamplerState = nullptr;
        /*
        Renderer::Shutdown();
        ForwardPlusRenderer::Shutdown();
        SceneManager::Shutdown();
        VirtualFileSystem::Shutdown();
        */
        AssetManager::Shutdown();
        Importer::Shutdown();
        AudioManager::Shutdown();
        ForwardRenderer::Shutdown();
        RenderAPI::Get().Shutdown();
    }

} // namespace Crowny
