#include "cwpch.h"

#include "Crowny/Application/Initializer.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include "Crowny/Scripting/CWMonoRuntime.h"
#include "Crowny/Scripting/Bindings/Logging/Debug.h"
#include "Crowny/Scripting/Bindings/Math/Transform.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
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
		Renderer::Init();
		Random::Init();
		FontManager::Add(CreateRef<Font>("Roboto Thin", "/Fonts/" + DEFAULT_FONT_FILENAME, 64));

		CWMonoRuntime::Init("Crowny C# Runtime");
		CWMonoRuntime::LoadAssemblies("/Assemblies");
		
		// TODO: Out of here
		Debug::InitRuntimeFunctions();
		ScriptTransform::InitRuntimeFunctions();
		ScriptComponent::InitRuntimeFunctions();
		ScriptEntity::InitRuntimeFunctions();
		ScriptTime::InitRuntimeFunctions();
	}

	void Initializer::Shutdown()
	{
		Renderer::Shutdown();
		Renderer2D::Shutdown();
		SceneManager::Shutdown();
		VirtualFileSystem::Shutdown();
	}

}