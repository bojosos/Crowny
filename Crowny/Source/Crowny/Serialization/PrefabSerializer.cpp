#include "cwpch.h"

#include "Crowny/Serialization/PrefabSerializer.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{

    PrefabSerializer::PrefabSerializer(const Ref<Prefab>& prefab) : m_Owner(prefab), m_Prefab(prefab.get()) {}

    PrefabSerializer::PrefabSerializer(Prefab& prefab) : m_Prefab(&prefab) {}

    String PrefabSerializer::SerializeToString()
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Prefab");

        out << YAML::BeginMap;

        SerializeValueYAML(out, "Version", (uint32_t)0);
        SerializeValueYAML(out, "PrefabName", m_Prefab->GetName());
        SerializeValueYAML(out, "RootEntityUuid", m_Prefab->GetRootEntityUuid());

        SceneSerializer sceneSerializer(m_Prefab->GetInternalScene());

        SerializeValueYAML(out, "Entities", YAML::BeginSeq);

        auto& scene = m_Prefab->GetInternalScene();
        scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });
        scene->m_Registry.each([&](auto entityID) {
            Entity entity = { entityID, scene.get() };
            if (!entity)
                return;
            // Skip the scene root — it's not part of the prefab data
            if (scene->GetRootEntity() == entity)
                return;
            sceneSerializer.SerializeEntity(out, entity);
        });
        out << YAML::EndSeq;

        out << YAML::EndMap;

        return out.c_str();
    }

    void PrefabSerializer::Serialize(const Path& filepath)
    {
        const String text = SerializeToString();
        String error;
        if (!FileSystem::WriteFileAtomic(filepath, reinterpret_cast<const byte*>(text.data()), text.size(), &error))
            throw std::runtime_error(error);
    }

    void PrefabSerializer::Deserialize(const Path& filepath) { DeserializeFromString(FileSystem::OpenFile(filepath)->GetAsString()); }

    void PrefabSerializer::DeserializeFromString(const String& text)
    {
        YAML::Node data = YAML::Load(text);

        const YAML::Node& prefabName = data["PrefabName"];
        if (prefabName)
            m_Prefab->SetName(prefabName.as<String>());

        const YAML::Node& rootUuid = data["RootEntityUuid"];
        if (rootUuid)
            m_Prefab->SetRootEntityUuid(rootUuid.as<UUID>());

        Ref<Scene>& scene = m_Prefab->GetInternalScene();
        scene = CreateRef<Scene>(false);
        scene->CreateRootEntity();

        const YAML::Node& entities = data["Entities"];
        if (!entities)
            return;

        SceneSerializer sceneSerializer(scene);
        sceneSerializer.DeserializeEntities(entities);
    }

} // namespace Crowny
