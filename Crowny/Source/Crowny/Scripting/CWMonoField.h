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
		const CWMonoType& GetType() const { return *m_Type; };
		CWMonoVisibility GetVisibility();
		void* GetValue();
		bool IsStatic() const;

		bool IsValueType();
		bool HasAttribute(CWMonoClass* monoClass);
		MonoObject* GetAttribute(CWMonoClass* monoClass);
		void Set(MonoObject* obj, void* value);
		void Get(MonoObject *obj, void* outval);

	private:
		MonoClassField* m_Field = nullptr;
		MonoVTable* m_OwningTypeVTable = nullptr;
		CWMonoType* m_Type;
		std::string m_Name;
		std::string m_FullDeclName;
	};
}