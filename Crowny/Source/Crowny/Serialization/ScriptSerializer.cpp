#include "cwpch.h"

#include "Crowny/Serialization/ScriptSerializer.h"

namespace Crowny
{

    // void ScriptSerializer::Serialize(SerializableScriptObject* object, Ref<MemoryDataStream>& stream)
    // {
    //     // YAML::Emitter out;
    //     // out << YAML::Comment("Crowny Scene");

    //     // out << YAML::BeginMap;
    //     // for (auto& field : object->m_Fields)
    //     // {
    //     //     Ref<SerializableFieldData> data = object->GetFieldData(field.second);
    //     //     out << YAML::Key << field.first->m_Field->GetName() << YAML::Value;
    //     //     field.second->Serialize(out);
    //     // }

    //     // out << YAML::EndSeq << YAML::EndMap;
    //     // m_Scene->m_Filepath = filepath;
    // }

    // Ref<SerializableScriptObject> ScriptSerializer::Deserialize(Ref<MemoryDataStream>& stream)
    // {
    //     // for (auto& field : )
    // }

} // namespace Crowny