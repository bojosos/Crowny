#include "cwpch.h"

#include "Crowny/Scripting/CWMonoAssembly.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Parser.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/appdomain.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/tokentype.h>
#include <mono/metadata/mono-debug.h>
END_MONO_INCLUDE

namespace Crowny
{

	static bool CheckImageOpenStatus(MonoImageOpenStatus status)
	{
		switch (status)
    	{
      		case MONO_IMAGE_OK:                  		                                                                           return true;
      		case MONO_IMAGE_ERROR_ERRNO: 		 CW_ENGINE_CRITICAL("MONO_IMAGE_ERROR_ERRNO while loading assembly");              return false;
      		case MONO_IMAGE_MISSING_ASSEMBLYREF: CW_ENGINE_CRITICAL("MONO_IMAGE_MISSING_ASSEMBLYREF while loading assembly"); 	   return false;
      		case MONO_IMAGE_IMAGE_INVALID: 		 CW_ENGINE_CRITICAL("MONO_IMAGE_IMAGE_INVALID while loading assembly");            return false;
		}

  		return false;
	}

	size_t CWMonoAssembly::ClassId::Hash::operator()(const CWMonoAssembly::ClassId& v) const
	{
		size_t seed = 0;
		HashCombine(seed, v.NamespaceName, v.Name);
		return seed;
	}
	
	bool CWMonoAssembly::ClassId::Equals::operator()(const CWMonoAssembly::ClassId& a, const CWMonoAssembly::ClassId& b) const
	{
		return a.NamespaceName == b.NamespaceName && a.Name == b.Name;
	}
  
  CWMonoAssembly::ClassId::ClassId(const std::string& namespaceName, const std::string& name)
          : Name(name), NamespaceName(namespaceName)
  {

  }

	CWMonoAssembly::CWMonoAssembly(const std::string& filepath, const std::string& name)
		: m_IsLoaded(false), m_AllClassesCached(false)
	{
		MonoImageOpenStatus status;
		auto [data, size] = VirtualFileSystem::Get()->ReadFile(filepath + "/" + name);
		m_Image = mono_image_open_from_data(reinterpret_cast<char*>(data), size, true, &status);
		CW_ENGINE_ASSERT(CheckImageOpenStatus(status) && m_Image);
    
		std::string dbgPath;
		if (VirtualFileSystem::Get()->ResolvePhyiscalPath(filepath + "/" + name + ".mdb", dbgPath))
		{
			CW_ENGINE_INFO("Loading debug: {0}", dbgPath);
			auto [data, size] = FileSystem::ReadFile(dbgPath);
			mono_debug_open_image_from_memory(m_Image, data, size);
		}

		m_Assembly = mono_assembly_load_from_full(m_Image, name.c_str(), &status, false);
		CW_ENGINE_ASSERT(CheckImageOpenStatus(status) && m_Assembly);
		m_IsLoaded = true;
	}

	CWMonoAssembly::CWMonoAssembly(MonoImage* image, const std::string& name) : m_AllClassesCached(false)
	{
		MonoAssembly* assembly = mono_image_get_assembly(image);
		CW_ENGINE_ASSERT(assembly, "Cannot get assembly from image");
		m_Assembly = assembly;
		m_Image = image;
		m_IsLoaded = true;
	}
	
	CWMonoAssembly::~CWMonoAssembly()
	{
		if (!m_IsLoaded)
			return;
		for (auto& cclass : m_Classes)
			delete cclass.second;
		
		if (m_Image)
			mono_image_close(m_Image);
		m_Classes.clear();
	}

	CWMonoClass* CWMonoAssembly::GetClass(const std::string& fullName) const
	{
		auto res = SplitString(fullName, ".");
		CW_ENGINE_ASSERT(res.size() == 2, "Name has to be in the format (Namespace.ClassName)");
		return GetClass(res[0], res[1]);
	}

	CWMonoClass* CWMonoAssembly::GetClass(const std::string& namespaceName, const std::string& className) const
	{
		CW_ENGINE_ASSERT(m_IsLoaded, "Assembly isn't loaded.");
		
		ClassId id(namespaceName, className);
		auto iter = m_Classes.find(id);
		
		if (iter != m_Classes.end())
			return iter->second;
		
		MonoClass* monoClass = mono_class_from_name(m_Image, namespaceName.c_str(), className.c_str());
		if (monoClass == nullptr)
			return nullptr;
		CWMonoClass* result = new CWMonoClass(monoClass);
		m_Classes[id] = result;
		return result;
	}

	const std::vector<CWMonoClass*>& CWMonoAssembly::GetClasses() const
	{
		if (m_AllClassesCached)
			return m_ClassList;

		m_ClassList.clear();
		std::stack<CWMonoClass*> todo;

		CWMonoAssembly* corlib = CWMonoRuntime::GetCorlibAssembly();
		CWMonoClass* compilerGeneratedAttrib = corlib->GetClass("System.Runtime.CompilerServices", "CompilerGeneratedAttribute");

		int numRows = mono_image_get_table_rows(m_Image, MONO_TABLE_TYPEDEF);

		for (int i = 1; i < numRows; i++) // #0 module
		{
			MonoClass* cclass = mono_class_get(m_Image, (i + 1) | MONO_TOKEN_TYPE_DEF);
			
			CWMonoClass* monoClass = new CWMonoClass(cclass);
			if (cclass)
			{
				if (monoClass->HasAttribute(compilerGeneratedAttrib))
					continue;

				todo.push(monoClass);
				while (!todo.empty())
				{
					CWMonoClass* nested = todo.top();
					todo.pop();

					void* iter = nullptr;
					do
					{
						MonoClass* cclass = mono_class_get_nested_types(nested->GetInternalPtr(), &iter);
						if (cclass == nullptr)
							break;
						if (cclass)
						{
						  std::string nestedType = nested->GetName() + "+" + mono_class_get_name(cclass);
					  	CWMonoClass* nestedClass = new CWMonoClass(cclass); // name might be wrong? not the nested one
							if (nestedClass->HasAttribute(compilerGeneratedAttrib))
								continue;
							m_ClassList.push_back(nestedClass);
							todo.push(nestedClass);
						}
					} while (true);
				}
				m_ClassList.push_back(monoClass);
			}
		}
		
		m_AllClassesCached = true;
		return m_ClassList;
	}

}
