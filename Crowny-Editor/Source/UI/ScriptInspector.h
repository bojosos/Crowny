#pragma once

namespace Crowny
{
    class ScriptInspector
    {
    public:
        static bool DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance,
                                        std::function<void(void*)> = {}, int depth = 0);

    private:
        static bool DrawPrimitiveInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label,
                                           std::function<MonoObject*()> getter, std::function<void(void*)> setter,
                                           const Ref<SerializableTypeInfo>& listTypeInfo = nullptr);
        static bool DrawFieldInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label,
                                       std::function<MonoObject*()> getter, std::function<void(void*)> setter,
                                       const Ref<SerializableTypeInfo>& listTypeInfo = nullptr, int depth = 0);
        static bool DrawListInspector(MonoObject* listObject, const Ref<SerializableMemberInfo>& memberInfo,
                                      std::function<void(void*)> setter, int depth = 0);
        static bool DrawDictionaryInspector(MonoObject* listObject, const Ref<SerializableMemberInfo>& memberInfo,
                                            std::function<void(void*)> setter, int depth = 0);
        static bool DrawEnumInspector(const Ref<SerializableMemberInfo>& memberInfo,
                                      const Ref<SerializableTypeInfoEnum>& enumInfo,
                                      std::function<MonoObject*()> getter, std::function<void(void*)> setter);
    };
} // namespace Crowny