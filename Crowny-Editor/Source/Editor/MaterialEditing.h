#pragma once

#include "Crowny/Renderer/BuiltInShaderCatalog.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MaterialPreset.h"
#include "Crowny/Scene/Scene.h"
#include "Editor/UndoRedo.h"

namespace Crowny
{
    // A shader change retains compatible authored values and texture assignments.
    inline bool ChangeMaterialShader(Material& material, const AssetHandle<Shader>& shader)
    {
        if (!shader || material.GetShader().GetHandleData() == shader.GetHandleData() ||
            !BuiltInShaderCatalog::IsMaterialShader(shader->GetName(), *shader))
            return false;
        const auto technique = shader->GetTechnique(ShaderVariation{});
        const bool decalShader =
          technique && std::find(technique->GetTags().begin(), technique->GetTags().end(), "material_model=decal") != technique->GetTags().end();
        if (decalShader != (material.GetDomain() == MaterialDomain::Decal))
            return false;
        const Ref<MaterialPreset> values = MaterialPreset::CaptureFromMaterial(material, "Previous shader");
        UnorderedMap<String, Pair<UniformResourceType, AssetHandle<Texture>>> textures;
        for (const auto& [name, descriptor] : material.GetTextureDescriptors())
        {
            const AssetHandle<Texture> texture = material.GetTextureHandle(name);
            if (name.rfind("cw_", 0) != 0 && texture.HasUUID())
                textures.emplace(name, std::pair{ descriptor.Type, texture });
        }
        material.SetShader(shader);
        material.ApplyModelDefaults();
        Vector<String> incompatible;
        for (const MaterialPresetParameter& parameter : values->GetParameters())
        {
            const auto binding = material.GetBindings().find(parameter.Name);
            if (binding == material.GetBindings().end() || binding->second.DataType != MaterialPresetValueTypeToShaderDataType(parameter.Type))
                incompatible.push_back(parameter.Name);
        }
        for (const String& name : incompatible)
            values->Remove(name);
        material.ApplyPreset(*values);
        for (const auto& [name, previous] : textures)
        {
            const auto texture = material.GetTextureDescriptors().find(name);
            if (texture != material.GetTextureDescriptors().end() && texture->second.Type == previous.first)
                material.SetTexture(name, previous.second);
        }
        if (material.HasAlphaModeOverride())
            material.SetFloat("alphaMode", static_cast<float>(material.GetAlphaMode()));
        return true;
    }

    class MaterialAssignmentAction final : public UndoAction
    {
    public:
        MaterialAssignmentAction(Entity entity, Vector<AssetHandle<Material>> before, Vector<AssetHandle<Material>> after, bool procedural)
          : UndoAction("Assign material"), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_Before(std::move(before)),
            m_After(std::move(after)), m_Procedural(procedural)
        {
        }
        void Commit() override { Apply(m_After); }
        void Revert() override { Apply(m_Before); }
        Entity GetFocusEntity() const override { return m_Scene->TryGetEntityFromUuid(m_Entity); }

    private:
        void Apply(const Vector<AssetHandle<Material>>& materials)
        {
            Entity entity = GetFocusEntity();
            if (!entity)
                return;
            if (m_Procedural && entity.HasComponent<ProceduralMeshComponent>())
                entity.GetComponent<ProceduralMeshComponent>().Materials = materials;
            else if (!m_Procedural && entity.HasComponent<MeshRendererComponent>())
                entity.GetComponent<MeshRendererComponent>().Materials = materials;
        }
        Ref<Scene> m_Scene;
        UUID m_Entity;
        Vector<AssetHandle<Material>> m_Before, m_After;
        bool m_Procedural;
    };

    // Object-ID picking identifies an entity, so a viewport drop replaces all its material slots.
    // Individual slots remain editable in the component inspector.
    inline Ref<UndoAction> AssignViewportMaterial(Entity entity, const AssetHandle<Material>& material)
    {
        if (!entity || !material || material->GetDomain() == MaterialDomain::Decal)
            return nullptr;
        Vector<AssetHandle<Material>>* materials = nullptr;
        bool procedural = false;
        if (entity.HasComponent<MeshRendererComponent>())
            materials = &entity.GetComponent<MeshRendererComponent>().Materials;
        else if (entity.HasComponent<ProceduralMeshComponent>())
        {
            materials = &entity.GetComponent<ProceduralMeshComponent>().Materials;
            procedural = true;
        }
        if (!materials || (!materials->empty() && std::all_of(materials->begin(), materials->end(), [&](const auto& previous) {
                return previous.GetHandleData() == material.GetHandleData();
            })))
            return nullptr;
        const Vector<AssetHandle<Material>> before = *materials;
        materials->assign(std::max(size_t(1), materials->size()), material);
        return CreateRef<MaterialAssignmentAction>(entity, before, *materials, procedural);
    }
} // namespace Crowny
