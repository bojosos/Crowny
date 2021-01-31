#include "cwpch.h"

#include "Crowny/Scripting/CWMonoAssembly.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Parser.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/appdomain.h>
#include <mono/metadata/metadata.h>
END_MONO_INCLUDE

namespace Crowny
{

	static bool CheckImageOpenStatus(MonoImageOpenStatus status)
	{
		switch (status)
    	{
      		case MONO_IMAGE_OK:                                                                                               return true;
      		case MONO_IMAGE_ERROR_ERRNO: CW_ENGINE_CRITICAL("MONO_IMAGE_ERROR_ERRNO while loading assembly");                 return false;
      		case MONO_IMAGE_MISSING_ASSEMBLYREF: CW_ENGINE_CRITICAL("MONO_IMAGE_MISSING_ASSEMBLYREF while loading assembly"); return false;
      		case MONO_IMAGE_IMAGE_INVALID: CW_ENGINE_CRITICAL("MONO_IMAGE_IMAGE_INVALID while loading assembly");             return false;
		}

  		return false;
	}

	CWMonoAssembly::CWMonoAssembly(MonoDomain* domain, const std::string& filepath)
	{
		MonoImageOpenStatus status;
		auto [data, size] = VirtualFileSystem::Get()->ReadFile(filepath);
		m_Image = mono_image_open_from_data(reinterpret_cast<char*>(data), size, true, &status);
		CW_ENGINE_ASSERT(CheckImageOpenStatus(status));
		m_Assembly = mono_assembly_load_from(m_Image, "Crowny assembly", &status);
		CW_ENGINE_ASSERT(CheckImageOpenStatus(status));
	}

	CWMonoClass* CWMonoAssembly::GetClass(const std::string& fullName)
	{
		auto res = SplitString(fullName, ".");
		CW_ENGINE_ASSERT(res.size() == 2, "Name has to be in the format (Namespace.ClassName)");
		return GetClass(res[0], res[1]);
	}

	CWMonoClass* CWMonoAssembly::GetClass(const std::string& namespaceName, const std::string& className)
	{
		auto it = m_Classes.find(namespaceName + "." + className);
		if (it == m_Classes.end())
		{
			MonoClass* monoClass = mono_class_from_name(m_Image, namespaceName.c_str(), className.c_str());
			if (monoClass)
			{
				m_Classes[namespaceName + "." + className] = new CWMonoClass(monoClass);
				return m_Classes[namespaceName + "." + className];
			}
			else
				return nullptr;
		}
		return it->second;
	}

}
