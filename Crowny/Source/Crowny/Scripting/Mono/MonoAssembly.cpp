#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoManager.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/tokentype.h>

namespace Crowny
{

    static bool CheckImageOpenStatus(MonoImageOpenStatus status)
    {
        switch (status)
        {
        case MONO_IMAGE_OK:
            return true;
        case MONO_IMAGE_ERROR_ERRNO:
            CW_ENGINE_CRITICAL("MONO_IMAGE_ERROR_ERRNO while loading assembly");
            return false;
        case MONO_IMAGE_MISSING_ASSEMBLYREF:
            CW_ENGINE_CRITICAL("MONO_IMAGE_MISSING_ASSEMBLYREF while loading assembly");
            return false;
        case MONO_IMAGE_IMAGE_INVALID:
            CW_ENGINE_CRITICAL("MONO_IMAGE_IMAGE_INVALID while loading assembly");
            return false;
        }

        return false;
    }

    size_t MonoAssembly::ClassId::Hash::operator()(const MonoAssembly::ClassId& v) const
    {
        size_t seed = 0;
        HashCombine(seed, v.NamespaceName, v.Name);
        return seed;
    }

    bool MonoAssembly::ClassId::Equals::operator()(const MonoAssembly::ClassId& a, const MonoAssembly::ClassId& b) const
    {
        return a.NamespaceName == b.NamespaceName && a.Name == b.Name;
    }

    MonoAssembly::ClassId::ClassId(const String& namespaceName, const String& name)
      : Name(name), NamespaceName(namespaceName)
    {
    }

    MonoAssembly::MonoAssembly(const Path& filepath, const String& name)
      : m_IsLoaded(false), m_AllClassesCached(false), m_Image(nullptr), m_Name(name), m_Path(filepath),
        m_Assembly(nullptr), m_IsDependency(false), m_DebugData(nullptr)
    {
    }

    MonoAssembly::~MonoAssembly() { Unload(); }

    void MonoAssembly::Load()
    {
        if (m_IsLoaded)
            Unload();

        Ref<DataStream> assemblyStream = FileSystem::OpenFile(m_Path);
        if (assemblyStream == nullptr)
        {
            CW_ENGINE_ERROR("Could not load assembly from {0}. Path does not exist.", m_Path);
            return;
        }

        uint32_t assemblySize = (uint32_t)assemblyStream->Size();
        char* assemblyData = new char[assemblySize];
        assemblyStream->Read(assemblyData, assemblySize);
        String imageName = m_Path.filename();
        MonoImageOpenStatus status = MONO_IMAGE_OK;
        MonoImage* image =
          mono_image_open_from_data_with_name(assemblyData, assemblySize, true, &status, false, imageName.c_str());
        delete[] assemblyData;

        if (status != MONO_IMAGE_OK || image == nullptr)
        {
            CW_ENGINE_ERROR("Failed to load image data from assembly: {0}", m_Path);
            return;
        }

        // #ifdef CW_DEBUG
        Path mdbPath = m_Path;
        m_Path.replace_extension(mdbPath.extension().string() + ".mdb");
        if (fs::exists(mdbPath))
        {
            CW_ENGINE_INFO("Loaded mdb");
            Ref<DataStream> mdbStream = FileSystem::OpenFile(mdbPath);
            if (mdbStream != nullptr)
            {
                uint32_t mdbSize = (uint32_t)mdbStream->Size();
                m_DebugData = new uint8_t[mdbSize];
                mdbStream->Read(m_DebugData, mdbSize);
                mono_debug_open_image_from_memory(image, m_DebugData, mdbSize);
            }
        }
        // #endif

        m_Assembly = mono_assembly_load_from_full(image, imageName.c_str(), &status, false);
        if (status != MONO_IMAGE_OK || m_Assembly == nullptr)
        {
            CW_ENGINE_ERROR("Failed to load assembly from: {0}", m_Path);
            return;
        }

        m_Image = image;
        if (m_Image == nullptr)
        {
            CW_ENGINE_ERROR("Failed to get assembly image: {0}", m_Path);
        }

        m_IsLoaded = true;
        m_IsDependency = false;
    }

    void MonoAssembly::Unload()
    {
        if (!m_IsLoaded)
            return;
        for (auto& entry : m_ClassesByRaw)
            delete entry.second;

        m_Classes.clear();
        m_ClassesByRaw.clear();
        m_ClassList.clear();
        m_AllClassesCached = false;

        if (!m_IsDependency)
        {
            if (m_DebugData != nullptr)
            {
                mono_debug_close_image(m_Image);
                delete[] m_DebugData;
                m_DebugData = nullptr;
            }
            if (m_Image)
            {
                mono_image_close(m_Image);
                m_Image = nullptr;
            }
            m_Assembly = nullptr;
            m_IsLoaded = false;
        }
    }

    void MonoAssembly::LoadFromImage(MonoImage* image)
    {
        ::MonoAssembly* assembly = mono_image_get_assembly(image);
        CW_ENGINE_ASSERT(assembly != nullptr);
        m_Assembly = assembly;
        m_Image = image;

        m_IsLoaded = true;
        m_IsDependency = true;
    }

    MonoClass* MonoAssembly::GetClass(const String& fullName) const
    {
        auto res = StringUtils::SplitString(fullName, ".");
        CW_ENGINE_ASSERT(res.size() == 2, "Name has to be in the format (Namespace.ClassName)");
        return GetClass(res[0], res[1]);
    }

    MonoClass* MonoAssembly::GetClass(const String& namespaceName, const String& className) const
    {
        CW_ENGINE_ASSERT(m_IsLoaded, "Assembly isn't loaded.");

        ClassId id(namespaceName, className);
        auto iter = m_Classes.find(id);

        if (iter != m_Classes.end())
            return iter->second;

        ::MonoClass* monoClass = mono_class_from_name(m_Image, namespaceName.c_str(), className.c_str());
        if (monoClass == nullptr)
            return nullptr;
        MonoClass* result = new MonoClass(monoClass);
        m_Classes[id] = result;
        return result;
    }

    MonoClass* MonoAssembly::GetClass(::MonoClass* rawClass) const
    {
        CW_ENGINE_ASSERT(m_IsLoaded);
        if (rawClass == nullptr)
            return nullptr;
        auto findIter = m_ClassesByRaw.find(rawClass);
        if (findIter != m_ClassesByRaw.end())
            return findIter->second;
        String ns;
        String typeName;
        MonoUtils::GetClassName(rawClass, ns, typeName);

        MonoImage* classImage = mono_class_get_image(rawClass); // Is it from this assembly
        if (classImage != m_Image)
            return nullptr;

        MonoClass* newClass = new MonoClass(rawClass);
        m_ClassesByRaw[rawClass] = newClass;
        MonoAssembly::ClassId classId(ns, typeName);
        m_Classes[classId] = newClass;

        return newClass;
    }

    const Vector<MonoClass*>& MonoAssembly::GetClasses() const
    {
        if (m_AllClassesCached)
            return m_ClassList;

        m_ClassList.clear();
        std::stack<MonoClass*> todo;

        MonoAssembly* corlib = MonoManager::Get().GetAssembly("corlib");
        MonoClass* compilerGeneratedAttrib =
          corlib->GetClass("System.Runtime.CompilerServices", "CompilerGeneratedAttribute");

        int numRows = mono_image_get_table_rows(m_Image, MONO_TABLE_TYPEDEF);

        for (int i = 1; i < numRows; i++) // #0 module
        {
            ::MonoClass* cclass = mono_class_get(m_Image, (i + 1) | MONO_TOKEN_TYPE_DEF);

            MonoClass* monoClass = new MonoClass(cclass);
            if (cclass)
            {
                if (monoClass->HasAttribute(compilerGeneratedAttrib))
                    continue;

                todo.push(monoClass);
                while (!todo.empty())
                {
                    MonoClass* nested = todo.top();
                    todo.pop();

                    void* iter = nullptr;
                    do
                    {
                        ::MonoClass* cclass = mono_class_get_nested_types(nested->GetInternalPtr(), &iter);
                        if (cclass == nullptr)
                            break;
                        if (cclass)
                        {
                            String nestedType = nested->GetName() + "+" + mono_class_get_name(cclass);
                            MonoClass* nestedClass = new MonoClass(cclass); // name might be wrong? not the nested one
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

} // namespace Crowny
