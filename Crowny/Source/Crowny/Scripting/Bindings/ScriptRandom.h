#pragma once

namespace Crowny
{
    class ScriptRandom
	{
	public:
		static void InitRuntimeFunctions();

	private:
		static void Internal_UnitSphere(glm::vec3* out);
		

    };
}