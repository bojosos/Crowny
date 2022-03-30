#pragma once

namespace Crowny
{

    class SerializableList
    {
    public:
        SerializableList(const Ref<SerializableTypeInfoList>& listInfo, MonoObject* instnace);
        ~SerializableList();

        Ref<SerializableList> CreateFromExisting(MonoObject* instance, const Ref<SerializableTypeInfoList>& listInfo);
        Ref<SerializableList> CreateNew(const Ref<SerializableTypeInfoList>& listInfo, uint32_t size);
        MonoObject* CreateManagedInstance(const Ref<SerializableTypeInfoList>& listInfo, uint32_t size);
        MonoObject* GetManagedInstance() const;
        Ref<SerializableList> CreateEmpty();

        void SetFieldData(uint32_t idx, const Ref<SerializableFieldData>& data);
        Ref<SerializableFieldData> GetFieldData(uint32_t idx);

    private:
        MonoObject* Deserialize();
        void InitMonoObjects(MonoClass* listClkass);
        size_t GetLength();
        void SetFieldData(MonoObject* object, uint32_t idx, const Ref<SerializableFieldData>& val);
        void AddFieldData(const Ref<SerializableFieldData>& val);

    private:
        MonoMethod* m_AddMethod = nullptr;
        MonoMethod* m_AddRangeMethod = nullptr;
        MonoMethod* m_ClearMethod = nullptr;
        MonoMethod* m_CopyToMethod = nullptr;
        MonoProperty* m_ItemProp = nullptr;
        MonoProperty* m_CountProp = nullptr;

        Ref<SerializableTypeInfoList> m_ListInfo;
        Vector<Ref<SerializableFieldData>> m_CachedEntries;
        uint32_t m_NumElements;
        uint32_t m_GCHandle;
    };

} // namespace Crowny