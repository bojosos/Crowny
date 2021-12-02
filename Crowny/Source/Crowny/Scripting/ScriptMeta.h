#pragma once

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoField.h"

namespace Crowny
{

    struct ScriptMeta
    {
        ScriptMeta() : ScriptClass(nullptr), CachedPtrField(nullptr){};
        ScriptMeta(const String& assembly, const String& ns, const String& name, std::function<void()> initCallback)
          : Assembly(assembly), Namespace(ns), InitCallback(initCallback), Name(name), ScriptClass(nullptr),
            CachedPtrField(nullptr)
        {
        }

        String Namespace;
        String Name;
        String Assembly;
        std::function<void()> InitCallback;
        MonoClass* ScriptClass;
        MonoField* CachedPtrField;
    };

} // namespace Crowny