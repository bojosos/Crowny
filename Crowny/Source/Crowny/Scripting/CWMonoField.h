#pragma once

#include "Crowny/Scripting/CWMonoVisibility.h"
#include "Crowny/Scripting/CWMonoType.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/object.h>
END_MONO_INCLUDE

namespace Crowny
{
	class CWMonoClass;
	
	class CWMonoField
	{
	public:
		CWMonoField(MonoClassField* field);
		const std::string& GetName() const { return m_Name; };
		const std::string& GetFullDeclName() const { return m_FullDeclName; }
		CWMonoClass* GetType() const { return m_Type; };
		CWMonoVisibility GetVisibility() const;
		bool IsStatic() const;

		bool IsValueType() const;
		bool HasAttribute(CWMonoClass* monoClass) const;
		MonoObject* GetAttribute(CWMonoClass* monoClass) const;
		void Set(MonoObject* obj, void* value);
		MonoObject* GetBoxed(MonoObject* instance);
		void Get(MonoObject *obj, void* outval);

	private:
		MonoClassField* m_Field = nullptr;
		MonoVTable* m_OwningTypeVTable = nullptr;
		CWMonoClass* m_Type;
		std::string m_Name;
		std::string m_FullDeclName;
	};
}