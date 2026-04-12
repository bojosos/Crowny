#pragma once

namespace Crowny
{
    struct FieldContext
    {
        Ref<SerializableMemberInfo> MemberInfo;
        std::function<MonoObject*()> Getter;
        std::function<void(void*)> Setter;
        Ref<SerializableTypeInfo> OverrideTypeInfo; // for list/dictionary element types
        int Depth = 0;

        const Ref<SerializableTypeInfo>& GetTypeInfo() const { return OverrideTypeInfo ? OverrideTypeInfo : MemberInfo->m_TypeInfo; }
    };

    class ScriptInspector
    {
    public:
        static bool DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance, std::function<void(void*)> = {},
                                        int depth = 0);

    private:
        static bool DrawPrimitiveInspector(const char* label, const FieldContext& ctx);
        static bool DrawFieldInspector(const char* label, const FieldContext& ctx);
        static bool DrawListInspector(MonoObject* listObject, const FieldContext& ctx);
        static bool DrawDictionaryInspector(MonoObject* dictObject, const FieldContext& ctx);
        static bool DrawEnumInspector(const FieldContext& ctx);

        static bool DrawStringField(const char* label, const FieldContext& ctx);
        static bool DrawMatrix4Field(const char* label, const FieldContext& ctx);
    };
} // namespace Crowny
