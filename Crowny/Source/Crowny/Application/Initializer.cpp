#include "cwpch.h"

#include "Crowny/Application/Initializer.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"

#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Scripting/Bindings/Math/ScriptTransform.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCameraComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptGameObject.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"
#include "Crowny/Scripting/Bindings/ScriptInput.h"
#include "Crowny/Scripting/Bindings/ScriptRandom.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{

    void Initializer::Init()
    {
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

        Renderer2D::Init();
        /*
        ForwardPlusRenderer::Init();
*/
        FontManager::Add(CreateRef<Font>("Roboto Thin", "/Fonts/" + DEFAULT_FONT_FILENAME, 64));
        CWMonoRuntime::Init("Crowny C# Runtime");
        CWMonoRuntime::LoadAssemblies("/Assemblies");

        // TODO: Out of here, maybe
        ScriptTransform::InitRuntimeFunctions();
        ScriptDebug::InitRuntimeFunctions();
        ScriptComponent::InitRuntimeFunctions();
        ScriptEntity::InitRuntimeFunctions();
        ScriptTime::InitRuntimeFunctions();
        ScriptRandom::InitRuntimeFunctions();
        ScriptInput::InitRuntimeFunctions();
        ScriptNoise::InitRuntimeFunctions();
        ScriptGameObject::InitRuntimeFunctions();
        ScriptCameraComponent::InitRuntimeFunctions();
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
        RenderAPI::Get().Shutdown();
        AudioManager::Shutdown();
    }

} // namespace Crowny
