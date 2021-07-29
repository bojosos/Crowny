#include "cwpch.h"

#include "Crowny/Application/Initializer.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RendererAPI.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/IDBufferRenderer.h"
#include "Crowny/Scene/SceneManager.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Scripting/CWMonoRuntime.h"
#include "Crowny/Scripting/Bindings/ScriptRandom.h"
#include "Crowny/Scripting/Bindings/ScriptInput.h"
#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Scripting/Bindings/Math/ScriptTransform.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCameraComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptGameObject.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"

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

		Random::StartUp(); // wat
		RendererAPI::StartUp<VulkanRendererAPI>();
		Renderer::Init();
		
		/*
		ForwardPlusRenderer::Init();
		Renderer2D::Init();
		IDBufferRenderer::Init();
		FontManager::Add(CreateRef<Font>("Roboto Thin", "/Fonts/" + DEFAULT_FONT_FILENAME, 64)); // default font, move out of here
*/
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
	{/*
		Renderer::Shutdown();
		Renderer2D::Shutdown();
		ForwardPlusRenderer::Shutdown();
		SceneManager::Shutdown();
		VirtualFileSystem::Shutdown();*/
		RendererAPI::Get().Shutdown();
	}

}
