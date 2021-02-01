#pragma once

#include "Crowny/Scripting/CWMonoClass.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/assembly.h>
END_MONO_INCLUDE

namespace Crowny
{
	class CWMonoAssembly
	{
		struct ClassId
		{
			struct Hash
			{
				size_t operator()(const ClassId& v) const;
			};

			struct Equals
			{
				bool operator()(const ClassId& a, const ClassId& b) const;
			};

			ClassId(const std::string& namespaceName, const std::string& name);

			std::string NamespaceName;
			std::string Name;
		};
	public:
		CWMonoAssembly(const std::string& filepath);
		CWMonoAssembly(MonoImage* image, const std::string& name);
		~CWMonoAssembly();
		CWMonoClass* GetClass(const std::string& namespaceName, const std::string& className) const;
		CWMonoClass* GetClass(const std::string& fullName) const;
		const std::vector<CWMonoClass*>& GetClasses() const;
	private:
		MonoAssembly* m_Assembly;
		MonoImage* m_Image;
		bool m_IsLoaded;
		
		mutable bool m_AllClassesCached;
		mutable std::vector<CWMonoClass*> m_ClassList;
		mutable std::unordered_map<ClassId, CWMonoClass*, ClassId::Hash, ClassId::Equals> m_Classes;
	};
}
