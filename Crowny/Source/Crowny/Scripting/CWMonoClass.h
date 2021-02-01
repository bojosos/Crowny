#pragma once

#include "Crowny/Scripting/CWMonoMethod.h"
#include "Crowny/Scripting/CWMonoField.h"
#include "Crowny/Scripting/CWMonoProperty.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
END_MONO_INCLUDE

namespace Crowny
{

	class CWMonoRuntime;

	class CWMonoClass
	{
		struct MethodId
		{
			struct Hash
			{
				size_t operator()(const MethodId& value) const;
			};

			struct Equals
			{
				bool operator()(const MethodId& a, const MethodId& b) const;
			};

			MethodId(const std::string& a, uint32_t paramCount);

			std::string Name;
			uint32_t NumParams;
		};
	public:
		CWMonoClass(MonoClass* monoClass);
		~CWMonoClass();
		const std::string& GetName() const { return m_Name; }
		const std::string& GetNamespace() const { return m_NamespaceName; }
		const std::string& GetFullName() const { return m_FullName; }

		MonoObject* CreateInstance() const;
		void AddInternalCall(const std::string& managed, const void* func);

		const std::vector<CWMonoMethod*>& GetMethods() const;
		const std::vector<CWMonoField*>& GetFields() const;
		const std::vector<CWMonoProperty*>& GetProperties() const;
		std::vector<CWMonoClass*> GetAttributes() const;

		CWMonoClass* GetBaseClass() const;
		MonoObject* GetAttribute(CWMonoClass* monoClass) const;

		bool HasAttribute(CWMonoClass* monoClass) const;
		bool HasField(const std::string& name) const;
		bool IsSubClassOf(CWMonoClass* monoClass) const;
		bool IsValueType() const;

		CWMonoMethod* GetMethod(const std::string& name, uint32_t argc = 0) const;
		CWMonoMethod* GetMethod(const std::string& name, const std::string& signature) const;
		CWMonoField* GetField(const std::string& name) const;
		CWMonoProperty* GetProperty(const std::string& name) const;

		MonoClass* GetInternalPtr() const { return m_Class; }

		friend class CWMonoRuntime;
	private:
		MonoClass* m_Class;
		std::string m_Name, m_NamespaceName, m_FullName;
		
		mutable bool m_AllMethodsCached, m_AllFieldsCached, m_AllPropertiesCached;

		mutable std::unordered_map<MethodId, CWMonoMethod*, MethodId::Hash, MethodId::Equals> m_Methods;
		mutable std::unordered_map<std::string, CWMonoField*> m_Fields;
		mutable std::unordered_map<std::string, CWMonoProperty*> m_Properties;
		
		mutable std::vector<CWMonoMethod*> m_MethodList;
		mutable std::vector<CWMonoField*> m_FieldList;
		mutable std::vector<CWMonoProperty*> m_PropertyList;
	};

}		