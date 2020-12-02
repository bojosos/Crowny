#pragma once

#include "Crowny/Scripting/CWMonoMethod.h"
#include "Crowny/Scripting/CWMonoField.h"
#include "Crowny/Scripting/CWMonoObject.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
END_MONO_INCLUDE

namespace Crowny
{

	class CWMonoRuntime;

	class CWMonoClass
	{
	public:
		CWMonoClass(MonoClass* monoClass);
		const std::string& GetName() const { return m_Name; }
		const std::string& GetNamespace() const { return m_NamespaceName; }

		MonoObject* CreateInstance();
		void AddInternalCall(const std::string& managed, const void* func);

		std::vector<CWMonoMethod*> GetMethods();
		std::vector<CWMonoField*> GetFields();

		CWMonoMethod* GetMethod(const std::string& nameWithArgs);
		CWMonoMethod* GetMethod(const std::string& name, uint32_t argc);

		CWMonoField* GetField(const std::string& name);

		friend class CWMonoRuntime;
	private:
		MonoClass* m_Class;
		std::string m_Name;
		std::string m_NamespaceName;
	};

}