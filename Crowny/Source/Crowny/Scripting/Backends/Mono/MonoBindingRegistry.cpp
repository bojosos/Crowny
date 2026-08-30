#include "cwpch.h"

#include "Crowny/Scripting/Backends/Mono/MonoBindingRegistry.h"

#include "Crowny/Scripting/Managed/ManagedComponentTypes.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAnimationClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAudioMixer.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMaterial.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial2D.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial3D.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAnimation.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCollider3D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptLight.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptMeshComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody3D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSpriteRenderer.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptText.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"

namespace Crowny
{
    void MonoBindingRegistry::LoadAssembly(const String& assemblyName)
    {
        if (assemblyName != CROWNY_ASSEMBLY)
            return;

        Clear();
        const MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
        if (assembly == nullptr)
        {
            CW_ENGINE_ERROR("CrownySharp assembly is not loaded.");
            return;
        }

        m_BuiltinTypes.Entity = assembly->GetClass(CROWNY_NS, "Entity");
        m_BuiltinTypes.EntityBehaviour = assembly->GetClass(CROWNY_NS, "EntityBehaviour");
        m_BuiltinTypes.MissingEntityBehaviour = assembly->GetClass(CROWNY_NS, "MissingEntityBehaviour");
        if (m_BuiltinTypes.Entity == nullptr || m_BuiltinTypes.EntityBehaviour == nullptr || m_BuiltinTypes.MissingEntityBehaviour == nullptr)
        {
            CW_ENGINE_ERROR("CrownySharp does not expose the required Mono scene wrapper types.");
            Clear();
            return;
        }

        RegisterComponents();
        RegisterAssets();
    }

    void MonoBindingRegistry::Clear()
    {
        m_BuiltinTypes = {};
        m_ComponentBindings.clear();
        m_AssetBindings.clear();
        m_AssetBindingsByType.clear();
    }

    MonoComponentBinding* MonoBindingRegistry::FindComponent(MonoReflectionType* type)
    {
        const auto binding = m_ComponentBindings.find(type);
        return binding == m_ComponentBindings.end() ? nullptr : &binding->second;
    }

    MonoAssetBinding* MonoBindingRegistry::FindAsset(MonoReflectionType* type)
    {
        const auto binding = m_AssetBindings.find(type);
        return binding == m_AssetBindings.end() ? nullptr : &binding->second;
    }

    MonoAssetBinding* MonoBindingRegistry::FindAsset(AssetType type)
    {
        const auto binding = m_AssetBindingsByType.find(static_cast<uint32_t>(type));
        return binding == m_AssetBindingsByType.end() ? nullptr : &binding->second;
    }

    void MonoBindingRegistry::RegisterComponents()
    {
#define CW_REGISTER_COMPONENT(managedName, nativeType, scriptType) RegisterComponent<nativeType, scriptType>();
        CW_MANAGED_COMPONENT_TYPES(CW_REGISTER_COMPONENT)
#undef CW_REGISTER_COMPONENT
    }

    void MonoBindingRegistry::RegisterAssets()
    {
        RegisterAsset<AnimationClip, ScriptAnimationClip>();
        RegisterAsset<AudioClip, ScriptAudioClip>();
        RegisterAsset<AudioMixer, ScriptAudioMixer>();
        RegisterAsset<PhysicsMaterial2D, ScriptPhysicsMaterial2D>();
        RegisterAsset<PhysicsMaterial3D, ScriptPhysicsMaterial3D>();
        RegisterAsset<Mesh, ScriptMesh>();
        RegisterAsset<Font, ScriptFont>();
        RegisterAsset<Material, ScriptMaterial>();
        RegisterAsset<Texture, ScriptTexture>();
    }
} // namespace Crowny
