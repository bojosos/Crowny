#pragma once

#include "Crowny/Scripting/CWMonoVisibility.h"
#include "Crowny/Scripting/CWMonoType.h"

namespace Crowny
{
	class CWMonoMethod
	{
	public:
		CWMonoMethod(MonoMethod* method);
		const std::string& GetName() const { return m_Name; };
		const std::string& GetFullDeclName() const { return m_FullDeclName; }
		std::vector<CWMonoType> GetParameterTypes();
		CWMonoType GetReturnType();
		bool IsStatic();
		bool IsVirtual();
		CWMonoVisibility GetVisibility();

		void Call(MonoObject* instance);

	private:
		MonoMethod* m_Method = nullptr;
		MonoMethodSignature* m_Signature = nullptr;
		std::string m_Name;
		std::string m_FullDeclName;
	};
}