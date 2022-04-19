#pragma once

namespace Crowny
{
	class ScriptInspector
	{
    public:
        static void DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance,
                                        MonoObject* parentInstance, std::function<void(MonoObject*)> = {}, int depth = 0);
		
    private:
        static void DrawPrimitiveInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label,
                                           std::function<MonoObject*()> getter, std::function<void(void*)> setter,
                                           const Ref<SerializableTypeInfo>& listTypeInfo = nullptr);
        static void DrawFieldInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label, std::function<MonoObject*()> getter,
                                       std::function<void(void*)> setter,
                                       const Ref<SerializableTypeInfo>& listTypeInfo = nullptr, int depth = 0);
        static void DrawListInspector(MonoObject* listObject, const Ref<SerializableMemberInfo>& memberInfo, int depth = 0);
	    static void DrawEnumInspector(const Ref<SerializableMemberInfo>& memberInfo, const Ref<SerializableTypeInfoEnum>& enumInfo, std::function<MonoObject* ()> getter,
		    std::function<void(void*)> setter);
	};
}