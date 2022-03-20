#pragma once

#include "Crowny/Common/Yaml.h"

namespace Crowny
{

	struct EditorSettings;

	class EditorSettingsSerializer
	{
	public:
		static void Serialize(const Ref<EditorSettings>& manifest, YAML::Emitter& out);
		static Ref<EditorSettings> Deserialize(const YAML::Node& node);
	};
}