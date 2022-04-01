#pragma once

#include "Crowny/Scripting/ScriptObject.h"

#include "Crowny/Utils/Compression.h"

namespace Crowny
{

	class ScriptCompression : public ScriptObject<ScriptCompression>
	{
	public:
		SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Compression")
		ScriptCompression();

	private:
		static uint64_t Internal_Compress(MonoArray* src, MonoArray* dest, CompressionMethod method);
	};
} // namespace Crowny
