#include "cwpch.h"

#include "Crowny/Serialization/AssetManifestSerializer.h"
#include "Crowny/Assets/AssetManifest.h"

namespace Crowny
{

	void AssetManifestSerializer::Serialize(const Ref<AssetManifest>& manifest, YAML::Emitter& out)
	{
		out << YAML::Comment("Crowny manifest");
		out << YAML::BeginMap;
		out << YAML::Key << "Manifest" << YAML::Value << manifest->m_Name;

		out << YAML::Key << "Assets" << YAML::Value;
		out << YAML::BeginSeq;
		for (auto uuidPath : manifest->m_UuidToFilepath)
		{
			out << YAML::BeginMap;
			out << YAML::Key << uuidPath.first << YAML::Value << uuidPath.second.string();
			out << YAML::EndMap;
		}

		out << YAML::EndSeq << YAML::EndMap;
	}

}