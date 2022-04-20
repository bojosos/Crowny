#include "cwpch.h"

#include "Crowny/Scripting/ScriptInfoManager.h"

#include "Crowny/Scripting/Mono/MonoManager.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"

#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{

    ScriptInfoManager::ScriptInfoManager() { RegisterComponents(); }

    void ScriptInfoManager::InitializeTypes()
    {

        MonoAssembly* corlib = MonoManager::Get().GetAssembly("corlib");
        if (corlib == nullptr)
            CW_ENGINE_ERROR("Corlib assembly not loaded.");
        MonoAssembly* crownyAssembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
        if (crownyAssembly == nullptr)
            CW_ENGINE_ERROR("Crowny assembly not loaded.");

        m_Builtin.SystemTypeClass = corlib->GetClass("System", "Type");
        if (m_Builtin.SystemTypeClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Type class.");
        m_Builtin.SystemArrayClass = corlib->GetClass("System", "Array");
        if (m_Builtin.SystemArrayClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Array class.");
        m_Builtin.SystemGenericDictionaryClass = corlib->GetClass("System.Collections.Generic", "Dictionary`2");
        if (m_Builtin.SystemGenericDictionaryClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Collections.Generic.Dictionary<TKey, TValue> class.");
        m_Builtin.SystemGenericListClass = corlib->GetClass("System.Collections.Generic", "List`1");
        if (m_Builtin.SystemGenericListClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Collections.Generic.List<T> class.");

		m_Builtin.Vector2 = crownyAssembly->GetClass(CROWNY_NS, "Vector2");
		if (m_Builtin.Vector2 == nullptr)
			CW_ENGINE_ERROR("Cannot find {0}.Vector2 class.", CROWNY_NS);

		m_Builtin.Vector3 = crownyAssembly->GetClass(CROWNY_NS, "Vector3");
		if (m_Builtin.Vector3 == nullptr)
			CW_ENGINE_ERROR("Cannot find {0}.Vector3 class.", CROWNY_NS);

		m_Builtin.Vector4 = crownyAssembly->GetClass(CROWNY_NS, "Vector4");
		if (m_Builtin.Vector4 == nullptr)
			CW_ENGINE_ERROR("Cannot find {0}.Vector4 class.", CROWNY_NS);

		m_Builtin.Matrix4 = crownyAssembly->GetClass(CROWNY_NS, "Matrix4");
		if (m_Builtin.Matrix4 == nullptr)
			CW_ENGINE_ERROR("Cannot find {0}.Matrix4 class.", CROWNY_NS);
		
        m_Builtin.ComponentClass = crownyAssembly->GetClass(CROWNY_NS, "Component");
        if (m_Builtin.ComponentClass == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.Component class.", CROWNY_NS);

        m_Builtin.EntityClass = crownyAssembly->GetClass(CROWNY_NS, "Entity");
        if (m_Builtin.EntityClass == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.EntityClass class.", CROWNY_NS);

        m_Builtin.EntityBehaviour = crownyAssembly->GetClass(CROWNY_NS, "EntityBehaviour");
        if (m_Builtin.EntityBehaviour == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.EntityBehaviour class.", CROWNY_NS);

        m_Builtin.SerializeFieldAttribute = crownyAssembly->GetClass(CROWNY_NS, "SerializeField");
        if (m_Builtin.SerializeFieldAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.SerializeField class.", CROWNY_NS);

        m_Builtin.RangeAttribute = crownyAssembly->GetClass(CROWNY_NS, "Range");
        if (m_Builtin.RangeAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.Range class.", CROWNY_NS);

        m_Builtin.NotNullAttribute = crownyAssembly->GetClass(CROWNY_NS, "NotNull");
        if (m_Builtin.NotNullAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.NotNull class.", CROWNY_NS);

        /*m_Builtin.StepAttribute = crownyAssembly->GetClass(CROWNY_NS, "Step");
        if (m_Builtin.StepAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.Step class.", CROWNY_NS);*/

        m_Builtin.ShowInInspectorAttribute = crownyAssembly->GetClass(CROWNY_NS, "ShowInInspector");
        if (m_Builtin.ShowInInspectorAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.ShowInInspector class.", CROWNY_NS);

        m_Builtin.HideInInspectorAttribute = crownyAssembly->GetClass(CROWNY_NS, "HideInInspector");
        if (m_Builtin.HideInInspectorAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.HideInInspector class.", CROWNY_NS);

        m_Builtin.SerializableObjectAtrribute = crownyAssembly->GetClass(CROWNY_NS, "SerializeObject");
        if (m_Builtin.SerializableObjectAtrribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.SerializeObject class.", CROWNY_NS);

        m_Builtin.DontSerializeFieldAttribute = crownyAssembly->GetClass(CROWNY_NS, "DontSerializeField");
        if (m_Builtin.DontSerializeFieldAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.DontSerializeField class.", CROWNY_NS);

		m_Builtin.RequireComponent = crownyAssembly->GetClass(CROWNY_NS, "RequireComponent");
		if (m_Builtin.RequireComponent == nullptr)
			CW_ENGINE_ERROR("Cannot find {0}.RequireComponentclass.", CROWNY_NS);

        m_Builtin.ScriptUtils = crownyAssembly->GetClass(CROWNY_NS, "ScriptUtils");
        if (m_Builtin.ScriptUtils == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.ScriptUtils class.", CROWNY_NS);

        m_Builtin.ScriptCompiler = crownyAssembly->GetClass(CROWNY_NS, "ScriptCompiler");
        if (m_Builtin.ScriptCompiler == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.ScriptCompiler class.", CROWNY_NS);
    }

    bool ScriptInfoManager::IsBasicType(MonoClass* klass)
    {
        return klass->GetFullName() == m_Builtin.Vector2->GetFullName() || klass->GetFullName() == m_Builtin.Vector3->GetFullName() || klass->GetFullName() == m_Builtin.Vector4->GetFullName() || klass->GetFullName() == m_Builtin.Matrix4->GetFullName();
    }

    void ScriptInfoManager::LoadAssemblyInfo(const String& assemblyName)
    {
        MonoAssembly* curAssembly = MonoManager::Get().GetAssembly(assemblyName);
        if (curAssembly == nullptr)
            return;
        Ref<SerializableAssemblyInfo> assemblyInfo = CreateRef<SerializableAssemblyInfo>();
        assemblyInfo->m_Name = assemblyName;

        m_AssemblyInfos[assemblyName] = assemblyInfo;
        // MonoClass* assetClass = ScriptAsset::GetMetaData()->ScriptClass;

        const Vector<MonoClass*>& allClasses = curAssembly->GetClasses();
        for (auto* klass : allClasses)
        {
            if (IsBasicType(klass))
				continue;
            const bool isSerializable =
              klass->IsSubClassOf(m_Builtin.ComponentClass) ||
              /*klass->IsSubClassOf(assetClass) ||*/ klass->HasAttribute(m_Builtin.SerializableObjectAtrribute);
            if (klass->IsSubClassOf(m_Builtin.EntityBehaviour))
                m_EntityBehaviourClasses[klass->GetName()] = klass;
            const bool isInspectable = klass->HasAttribute(m_Builtin.ShowInInspectorAttribute);
            if ((isSerializable || isInspectable) &&
                klass != m_Builtin.ComponentClass /* && klass != m_Builtin.AssetClass*/)
            {
                Ref<SerializableTypeInfoObject> typeInfo = CreateRef<SerializableTypeInfoObject>();
                typeInfo->m_TypeNamespace = klass->GetNamespace();
                typeInfo->m_TypeName = klass->GetName();
                typeInfo->m_TypeId = m_UniqueTypeId++;

                if (isSerializable)
                    typeInfo->m_Flags |= ScriptFieldFlagBits::Serializable;
                if (isInspectable || isSerializable)
                    typeInfo->m_Flags |= ScriptFieldFlagBits::Inspectable;
                MonoPrimitiveType monoPrimitiveType = MonoUtils::GetPrimitiveType(klass->GetInternalPtr());
                if (monoPrimitiveType == MonoPrimitiveType::ValueType)
                    typeInfo->m_ValueType = true;
                else
                    typeInfo->m_ValueType = false;
                MonoReflectionType* type = MonoUtils::GetType(klass->GetInternalPtr());
                ScriptTypeInfo* reflTypeInfo = GetSerializableTypeInfo(type);
                /*if (reflTypeInfo != nullptr)
                    typeInfo->m_TypeId = reflTypeInfo->TypeId;
                else
                    typeInfo->m_TypeId = 0;*/
                Ref<SerializableObjectInfo> objInfo = CreateRef<SerializableObjectInfo>();
                objInfo->m_TypeInfo = typeInfo;
                objInfo->m_MonoClass = klass;
                assemblyInfo->m_TypeNameToId[objInfo->GetFullTypeName()] = typeInfo->m_TypeId;
                assemblyInfo->m_ObjectInfos[typeInfo->m_TypeId] = objInfo;
            }
        }

        // Fields and properties
        for (auto& curClassInfo : assemblyInfo->m_ObjectInfos)
        {
            Ref<SerializableObjectInfo> objInfo = curClassInfo.second;
            uint32_t m_UniqueFieldId = 1;
            const Vector<MonoField*>& fields = objInfo->m_MonoClass->GetFields();
            for (auto& field : fields)
            {
                if (field->IsStatic())
					continue;
                Ref<SerializableTypeInfo> typeInfo = GetTypeInfo(field->GetType());
                if (typeInfo == nullptr)
                    continue;
                bool isSerializable = true;
                bool isInspectable = true;
                if (typeInfo->GetType() == SerializableType::Object)
                {
                    SerializableTypeInfoObject* objTypeInfo = static_cast<SerializableTypeInfoObject*>(typeInfo.get());
                    isSerializable = objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Serializable);
                    isInspectable = isSerializable || objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable);
                }

                Ref<SerializableFieldInfo> fieldInfo = CreateRef<SerializableFieldInfo>();
                fieldInfo->m_FieldId = m_UniqueFieldId++;
                fieldInfo->m_Name = field->GetName();
                fieldInfo->m_Field = field;
                fieldInfo->m_TypeInfo = typeInfo;
                fieldInfo->m_ParentTypeId = objInfo->m_TypeInfo->m_TypeId;

                CrownyMonoVisibility visibility = field->GetVisibility();
                if (visibility == CrownyMonoVisibility::Public)
                {
                    if (isSerializable && !field->HasAttribute(m_Builtin.DontSerializeFieldAttribute))
                        fieldInfo->m_Flags |= ScriptFieldFlagBits::Serializable;
                    if (isInspectable && !field->HasAttribute(m_Builtin.HideInInspectorAttribute))
                        fieldInfo->m_Flags |= ScriptFieldFlagBits::Inspectable;
                }
                else
                {
                    if (isSerializable && field->HasAttribute(m_Builtin.SerializeFieldAttribute))
                        fieldInfo->m_Flags |= ScriptFieldFlagBits::Serializable;
                    if (isInspectable && field->HasAttribute(m_Builtin.ShowInInspectorAttribute))
                        fieldInfo->m_Flags |= ScriptFieldFlagBits::Inspectable;
                }

                if (field->HasAttribute(m_Builtin.RangeAttribute))
                    fieldInfo->m_Flags |= ScriptFieldFlagBits::Range;
                /*if (field->HasAttribute(m_Builtin.StepAttribute))
                    fieldInfo->m_Flags |= ScriptFieldFlagBits::Step;*/
                if (field->HasAttribute(m_Builtin.NotNullAttribute))
                    fieldInfo->m_Flags |= ScriptFieldFlagBits::NotNull;
                objInfo->m_FieldNameToId[fieldInfo->m_Name] = fieldInfo->m_FieldId;
                objInfo->m_Fields[fieldInfo->m_FieldId] = fieldInfo;
            }

            const Vector<MonoProperty*>& properties = objInfo->m_MonoClass->GetProperties();
            for (auto& property : properties)
            {
                Ref<SerializableTypeInfo> typeInfo = GetTypeInfo(property->GetReturnType());
                if (typeInfo == nullptr)
                    continue;
                bool isSerializable = true;
                bool isInspectable = true;
                if (typeInfo->GetType() == SerializableType::Object)
                {
                    SerializableTypeInfoObject* objTypeInfo = static_cast<SerializableTypeInfoObject*>(typeInfo.get());
                    isSerializable = objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Serializable);
                    isInspectable = isSerializable || objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable);
                }

                Ref<SerializablePropertyInfo> propertyInfo = CreateRef<SerializablePropertyInfo>();
                propertyInfo->m_FieldId = m_UniqueFieldId++;
                propertyInfo->m_Name = property->GetName();
                propertyInfo->m_Property = property;
                propertyInfo->m_TypeInfo = typeInfo;
                propertyInfo->m_ParentTypeId = objInfo->m_TypeInfo->m_TypeId;

                CrownyMonoVisibility visibility = property->GetVisibility();
                if (visibility == CrownyMonoVisibility::Public)
                {
                    if (isSerializable && !property->HasAttribute(m_Builtin.DontSerializeFieldAttribute))
                        propertyInfo->m_Flags |= ScriptFieldFlagBits::Serializable;
                    if (isInspectable && !property->HasAttribute(m_Builtin.HideInInspectorAttribute))
                        propertyInfo->m_Flags |= ScriptFieldFlagBits::Inspectable;
                }
                else
                {
                    if (isSerializable && property->HasAttribute(m_Builtin.SerializeFieldAttribute))
                        propertyInfo->m_Flags |= ScriptFieldFlagBits::Serializable;
                    if (isInspectable && property->HasAttribute(m_Builtin.ShowInInspectorAttribute))
                        propertyInfo->m_Flags |= ScriptFieldFlagBits::Inspectable;
                }

                if (property->HasAttribute(m_Builtin.RangeAttribute))
                    propertyInfo->m_Flags |= ScriptFieldFlagBits::Range;
                /*if (property->HasAttribute(m_Builtin.StepAttribute))
                    propertyInfo->m_Flags |= ScriptFieldFlagBits::Step;*/
                if (property->HasAttribute(m_Builtin.NotNullAttribute))
                    propertyInfo->m_Flags |= ScriptFieldFlagBits::NotNull;
                objInfo->m_FieldNameToId[propertyInfo->m_Name] = propertyInfo->m_FieldId;
                objInfo->m_Fields[propertyInfo->m_FieldId] = propertyInfo;
            }
        }

        for (auto& klass : assemblyInfo->m_ObjectInfos)
        {
            MonoClass* base = klass.second->m_MonoClass->GetBaseClass();
            while (base != nullptr)
            {
                Ref<SerializableObjectInfo> baseObjInfo;
                if (GetSerializableObjectInfo(base->GetNamespace(), base->GetName(), baseObjInfo))
                {
                    klass.second->m_BaseClass = baseObjInfo;
                    baseObjInfo->m_DerivedClasses.push_back(klass.second);
                    break;
                }
                base = base->GetBaseClass();
            }
        }
    }

    void ScriptInfoManager::RegisterComponents()
    {
        RegisterComponent<TransformComponent, ScriptTransform>();
        RegisterComponent<CameraComponent, ScriptCamera>();
        RegisterComponent<MonoScriptComponent, ScriptEntityBehaviour>();
        RegisterComponent<AudioSourceComponent, ScriptAudioSource>();
        RegisterComponent<AudioListenerComponent, ScriptAudioListener>();
        RegisterComponent<Rigidbody2DComponent, ScriptRigidbody2D>();
    }

    Ref<SerializableTypeInfo> ScriptInfoManager::GetTypeInfo(MonoClass* monoClass)
    {
        CW_ENGINE_ASSERT(m_BaseTypesInitialized);
        MonoPrimitiveType primitiveType = MonoUtils::GetPrimitiveType(monoClass->GetInternalPtr());
        bool isEnum = MonoUtils::IsEnum(monoClass->GetInternalPtr());
        if (isEnum)
            primitiveType = MonoUtils::GetEnumPrimitiveType(monoClass->GetInternalPtr());
        ScriptPrimitiveType scriptPrimitiveType = ScriptPrimitiveType::U32;
        bool isSimpleType = IsBasicType(monoClass);
        switch (primitiveType)
        {
        case MonoPrimitiveType::Bool:
            scriptPrimitiveType = ScriptPrimitiveType::Bool;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::Char:
            scriptPrimitiveType = ScriptPrimitiveType::Char;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::I8:
            scriptPrimitiveType = ScriptPrimitiveType::I8;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::U8:
            scriptPrimitiveType = ScriptPrimitiveType::U8;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::I16:
            scriptPrimitiveType = ScriptPrimitiveType::I16;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::U16:
            scriptPrimitiveType = ScriptPrimitiveType::U16;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::I32:
            scriptPrimitiveType = ScriptPrimitiveType::I32;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::U32:
            scriptPrimitiveType = ScriptPrimitiveType::U32;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::I64:
            scriptPrimitiveType = ScriptPrimitiveType::I64;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::U64:
            scriptPrimitiveType = ScriptPrimitiveType::U64;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::Float:
            scriptPrimitiveType = ScriptPrimitiveType::Float;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::Double:
            scriptPrimitiveType = ScriptPrimitiveType::Double;
            isSimpleType = true;
            break;
        case MonoPrimitiveType::String:
            scriptPrimitiveType = ScriptPrimitiveType::String;
            isSimpleType = true;
            break;
        default:
            break;
        }

        if (isSimpleType)
        {
            if (isEnum)
            {
                Ref<SerializableTypeInfoEnum> typeInfo =
                  CreateRef<SerializableTypeInfoEnum>(); // TODO: Retrieve enum names using the C# helper
                typeInfo->m_UnderlyingType = scriptPrimitiveType;
                typeInfo->m_TypeNamespace = monoClass->GetNamespace();
                typeInfo->m_TypeName = monoClass->GetName();

                void* enumType = (void*)MonoUtils::GetType(monoClass->GetInternalPtr());
                MonoArray* ar = (MonoArray*)ScriptInfoManager::Get()
                                  .GetBuiltinClasses()
                                  .ScriptUtils->GetMethod("GetEnumNames", 1)
                                  ->Invoke(nullptr, &enumType);
                size_t size = mono_array_length(ar);
                typeInfo->m_EnumNames.resize(size);
                for (uint32_t i = 0; i < size; i++)
                    typeInfo->m_EnumNames[i] = MonoUtils::FromMonoString(mono_array_get(ar, MonoString*, i));

                return typeInfo;
            }
            else
            {
				Ref<SerializableTypeInfoPrimitive> typeInfo = CreateRef<SerializableTypeInfoPrimitive>();
				if (monoClass->GetFullName() == m_Builtin.Vector2->GetFullName())
					scriptPrimitiveType = ScriptPrimitiveType::Vector2;
				else if (monoClass->GetFullName() == m_Builtin.Vector3->GetFullName())
					scriptPrimitiveType = ScriptPrimitiveType::Vector3;
				else if (monoClass->GetFullName() == m_Builtin.Vector4->GetFullName())
					scriptPrimitiveType = ScriptPrimitiveType::Vector4;
				else if (monoClass->GetFullName() == m_Builtin.Matrix4->GetFullName())
					scriptPrimitiveType = ScriptPrimitiveType::Matrix4;
                typeInfo->m_Type = scriptPrimitiveType;
                return typeInfo;
            }
        }

        switch (primitiveType)
        {
            // case MonoPrimitiveType::Class:
            //     if (monoClass->IsSubclassOf(ScriptAsset::GetMetaData()->ScriptClass)) // Asset
            //     {
            //         Ref<SerializableTypeInfoRef> typeInfo = CreateRef<SerializableTypeInfoRef>();
            //         typeInfo->m_TypeNamespace = monoClass->GetNamespace();
            //         typeInfo->m_TypeName = monoClass->GetName();
            //         typeInfo->m_TypeId = 0;

            //         if (monoClass == ScriptAsset::GetMetaData()->ScriptClass) // Asset handle
            //             typeInfo->m_Type = ScriptReferenceType::AssetBase;
            //         else // Specific asset (Texture, AudioClip...)
            //         {
            //             typeInfo->m_Type = ScriptReferenceType::Asset;
            //             MonoReflectionType* type = MonoUtils::GetType(monoClass->GetInternalPtr());
            //             AssetInfo* assetInfo = GetBuiltinAssetInfo(type);
            //             CW_ENGINE_ASSERT(builtinInfo != nullptr);
            //             typeInfo->m_TypeId = assetInfo->TypeId;
            //         }
            //         return typeInfo;
            //     }
            //     else
            //     else if (monoClass->IsSubClassOf(m_Builtin.ComponentClass))
            //     {
            //         if (monoClass == m_Builtin.ComponentClass)
            //             typeInfo->m_Type = ScriptReferenceType::ComponentBase;
            //         else
            //         {
            //             typeInfo->m_Type = ScriptReferenceType::Component;
            //             MonoReflectionType* type = MonoUtils::GetType(monoClass->GetInternalPtr());
            //             ComponentInfo* componentInfo = GetComponentInfo(type);
            //             CW_ENGINE_ASSERT(componentInfo != nullptr);
            //             typeInfo->m_TypeId = componentInfo->TypeId;
            //         }
            //     }
            //     else
            //     {
            // normal object
            //     }
        case MonoPrimitiveType::Class: {
            if (monoClass->GetFullName() == m_Builtin.EntityClass->GetFullName()) // Entity
            {
                Ref<SerializableTypeInfoEntity> typeInfo = CreateRef<SerializableTypeInfoEntity>();
                typeInfo->m_TypeNamespace = monoClass->GetNamespace();
                typeInfo->m_TypeName = monoClass->GetName();
                return typeInfo;
            } // Do components here
            else
            {
                Ref<SerializableObjectInfo> objInfo;
                if (GetSerializableObjectInfo(monoClass->GetNamespace(), monoClass->GetName(), objInfo))
                    return objInfo->m_TypeInfo;
            }
            break;
        }
        case MonoPrimitiveType::ValueType: {
            Ref<SerializableObjectInfo> objInfo;
            if (GetSerializableObjectInfo(monoClass->GetNamespace(), monoClass->GetName(), objInfo))
                return objInfo->m_TypeInfo;
            return nullptr;
        }
        case MonoPrimitiveType::Generic:
            if (monoClass->GetFullName() == m_Builtin.SystemGenericListClass->GetFullName())
            {
                Ref<SerializableTypeInfoList> typeInfo = CreateRef<SerializableTypeInfoList>();
                MonoProperty* itemProperty = monoClass->GetProperty("Item");
                MonoClass* itemClass = itemProperty->GetReturnType();
                if (itemClass != nullptr)
                    typeInfo->m_ElementType = GetTypeInfo(itemClass);
                typeInfo->m_Class = monoClass->GetInternalPtr();
                if (typeInfo->m_ElementType != nullptr)
                    return typeInfo;
                return nullptr;
            }
            else if (monoClass->GetFullName() == m_Builtin.SystemGenericDictionaryClass->GetFullName())
            {
                Ref<SerializableTypeInfoDictionary> typeInfo = CreateRef<SerializableTypeInfoDictionary>();
                MonoMethod* enumerator = monoClass->GetMethod("GetEnumerator");
                MonoClass* enumClass = enumerator->GetReturnType();

                MonoProperty* currentProperty = enumClass->GetProperty("Current");
                MonoClass* kvp = currentProperty->GetReturnType();

                MonoProperty* keyProperty = kvp->GetProperty("Key");
                MonoProperty* valueProperty = kvp->GetProperty("Value");

                MonoClass* keyClass = keyProperty->GetReturnType();
                if (keyClass != nullptr)
                    typeInfo->m_KeyType = GetTypeInfo(keyClass);

                MonoClass* valueClass = valueProperty->GetReturnType();
                if (valueClass != nullptr)
                    typeInfo->m_ValueType = GetTypeInfo(valueClass);
                typeInfo->m_Class = monoClass->GetInternalPtr();
                if (typeInfo->m_KeyType != nullptr && typeInfo->m_ValueType != nullptr)
                    return typeInfo;
                return nullptr;
            }
        case MonoPrimitiveType::Array:
        {
            Ref<SerializableTypeInfoArray> typeInfo = CreateRef<SerializableTypeInfoArray>();
            /*::MonoClass* elementClass = ScriptArray::GetElementClass(monoClass->GetInternalPtr());
            if (elementClass != nullptr)
            {
                MonoClass* monoElementClass = MonoManager::Get().FindClass(elementClass);
                if (monoElementClass != nullptr)
                    typeInfo->m_ElementType = GetTypeInfo(monoElementClass);
            }
            if (typeInfo->m_ElementType == nullptr)
                return nullptr;*/
            // typeInfo->m_Rank = ScriptArray::GetRank(monoClass->GetInternalPtr());
            return typeInfo;
        }
        default:
            break;
        }
        return nullptr;
    }

    bool ScriptInfoManager::GetSerializableObjectInfo(const String& ns, const String& name,
                                                      Ref<SerializableObjectInfo>& outInfo)
    {

        String fullName = ns + "." + name;
        for (auto& curAssembly : m_AssemblyInfos)
        {
            if (curAssembly.second == nullptr)
                continue;
            auto findIter = curAssembly.second->m_TypeNameToId.find(fullName);
            if (findIter != curAssembly.second->m_TypeNameToId.end())
            {
                outInfo = curAssembly.second->m_ObjectInfos[findIter->second];
                return true;
            }
        }
        return false;
    }

    void ScriptInfoManager::ClearScriptObjects()
    {
        m_BaseTypesInitialized = false;
        m_Builtin = BuiltinScriptClasses();
    }

    void ScriptInfoManager::ClearAssemblyInfo()
    {
        ClearScriptObjects();
        m_ComponentInfos.clear();
        m_AssemblyInfos.clear();
    }

    ScriptTypeInfo* ScriptInfoManager::GetSerializableTypeInfo(MonoReflectionType* type)
    {
        auto findIter = m_ScriptTypeInfos.find(type);
        if (findIter == m_ScriptTypeInfos.end())
            return nullptr;
        return &findIter->second;
    }

    ComponentInfo* ScriptInfoManager::GetComponentInfo(MonoReflectionType* type)
    {
        auto findIter = m_ComponentInfos.find(type);
        if (findIter != m_ComponentInfos.end())
            return &findIter->second;
        return nullptr;
    }

} // namespace Crowny