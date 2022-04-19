#include "cwpch.h"

#include "Crowny/Serialization/ScriptSerializer.h"

namespace Crowny
{

	void Save(BinaryDataStreamOutputArchive& archive, const SerializableObject& object)
	{
		// archive(object.m_ObjectInfo);
		Ref<SerializableObjectInfo> objInfo = object.m_ObjectInfo;
		while (objInfo != nullptr)
		{
			for (auto [id, member] : objInfo->m_Fields)
			{
				if (member->IsSerializable())
				{
				//	object.GetFieldData()
				}
			}
			objInfo = objInfo->m_BaseClass;
		}
	}

	void ScriptSerializer::Serialize(SerializableObject* object)
	{
		/*		
		YAML::Emitter out;
		out << YAML::Comment("Crowny Scene");

		out << YAML::BeginMap;
		for (auto& field : object->m_ObjectInfo)
		{
			Ref<SerializableFieldData> data = object->GetFieldData(field.second);
			out << YAML::Key << field.first->m_Field->GetName() << YAML::Value;
			field.second->Serialize(out);
		}

		out << YAML::EndSeq << YAML::EndMap;
		m_Scene->m_Filepath = filepath;*/
	}

	// Ref<SerializableScriptObject> ScriptSerializer::Deserialize(Ref<MemoryDataStream>& stream)
	 // {
		 // for (auto& field : )
	 // }

} // namespace Crowny