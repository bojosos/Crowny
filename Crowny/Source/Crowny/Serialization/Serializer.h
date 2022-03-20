#pragma once

#include "Crowny/Common/Yaml.h"

namespace Crowny
{
	class TextSerializer
	{
		template <typename T>
		static YAML::Emitter Serialize(const Ref<T>& serializable);
	};
}