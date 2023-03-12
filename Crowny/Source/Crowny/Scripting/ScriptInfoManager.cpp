#include "cwpch.h"

#include "Crowny/Scripting/ScriptInfoManager.h"

#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"
#include "Crowny/Scripting/ScriptComponent.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"

#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

#define LOAD_CW_CLASS(CLASS_NAME)                                                                                      \
    m_Builtin.##CLASS_NAME = crownyAssembly->GetClass(CROWNY_NS, #CLASS_NAME);                                         \
    if (m_Builtin.##CLASS_NAME == nullptr)                                                                             \
        CW_ENGINE_ERROR("Cannot find {0}." #CLASS_NAME " class.", CROWNY_NS);

#define LOAD_CW_ATTR(CLASS_NAME)                                                                                       \
    m_Builtin.##CLASS_NAME##Attribute = crownyAssembly->GetClass(CROWNY_NS, #CLASS_NAME);                              \
    if (m_Builtin.CLASS_NAME##Attribute == nullptr)                                                                    \
        CW_ENGINE_ERROR("Cannot find {0}." #CLASS_NAME " attribute.", CROWNY_NS);

#define LOAD_SYSTEM_CLASS(CLASS_NAME)                                                                                  \
    m_Builtin.##System##CLASS_NAME##Class = corlib->GetClass("System", #CLASS_NAME);                                   \
    if (m_Builtin.##System##CLASS_NAME##Class == nullptr)                                                              \
        CW_ENGINE_ERROR("Cannot find {0}." #CLASS_NAME " class.", "System");

namespace Crowny
{

    ScriptInfoManager::ScriptInfoManager()
    {
        // RegisterComponents();
        // RegisterAssets();
    }

    void ScriptInfoManager::InitializeTypes()
    {
        MonoAssembly* corlib = MonoManager::Get().GetAssembly("corlib");
        if (corlib == nullptr)
            CW_ENGINE_ERROR("Corlib assembly not loaded.");
        MonoAssembly* crownyAssembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
        if (crownyAssembly == nullptr)
            CW_ENGINE_ERROR("Crowny assembly not loaded.");

        m_Builtin.SystemGenericDictionaryClass = corlib->GetClass("System.Collections.Generic", "Dictionary`2");
        if (m_Builtin.SystemGenericDictionaryClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Collections.Generic.Dictionary<TKey, TValue> class.");
        m_Builtin.SystemGenericListClass = corlib->GetClass("System.Collections.Generic", "List`1");
        if (m_Builtin.SystemGenericListClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Collections.Generic.List<T> class.");

        LOAD_SYSTEM_CLASS(Type);
        LOAD_SYSTEM_CLASS(Array);

        LOAD_CW_CLASS(Vector2);
        LOAD_CW_CLASS(Vector3);
        LOAD_CW_CLASS(Vector4);
        LOAD_CW_CLASS(Matrix4);
        LOAD_CW_CLASS(Color);
        LOAD_CW_CLASS(Component);
        LOAD_CW_CLASS(Entity);
        LOAD_CW_CLASS(EntityBehaviour);

        LOAD_CW_ATTR(SerializeField);
        LOAD_CW_ATTR(Range);
        LOAD_CW_ATTR(NotNull);
        // LOAD_CW_ATTR(Step);
        LOAD_CW_ATTR(ShowInInspector);
        LOAD_CW_ATTR(HideInInspector);
        LOAD_CW_ATTR(SerializeObject);
        LOAD_CW_ATTR(DontSerializeField);
        LOAD_CW_ATTR(RequireComponent);
        LOAD_CW_ATTR(Dropdown);
        LOAD_CW_ATTR(Label);
        LOAD_CW_ATTR(Filepath);
        LOAD_CW_ATTR(ReadOnly);
        LOAD_CW_ATTR(Multiline);
        LOAD_CW_ATTR(ColorUsage);
        LOAD_CW_ATTR(ColorPalette);
        LOAD_CW_ATTR(EnumQuickTabs);

        LOAD_CW_ATTR(Header);

        LOAD_CW_CLASS(ScriptUtils);
        LOAD_CW_CLASS(ScriptCompiler);

        RegisterComponents();
        RegisterAssets();

        m_BaseTypesInitialized = true;
    }

    bool ScriptInfoManager::IsBasicType(MonoClass* klass)
    {
        return klass->GetFullName() == m_Builtin.Vector2->GetFullName() ||
               klass->GetFullName() == m_Builtin.Vector3->GetFullName() ||
               klass->GetFullName() == m_Builtin.Vector4->GetFullName() ||
               klass->GetFullName() == m_Builtin.Matrix4->GetFullName();
    }

    void ScriptInfoManager::LoadAssemblyInfo(const String& assemblyName)
    {
        if (!m_BaseTypesInitialized && assemblyName == "CrownySharp")
            InitializeTypes();

        MonoAssembly* curAssembly = MonoManager::Get().GetAssembly(assemblyName);
        if (curAssembly == nullptr)
            return;

        uint32_t m_UniqueTypeId = 1;
        Ref<SerializableAssemblyInfo> assemblyInfo = CreateRef<SerializableAssemblyInfo>();
        assemblyInfo->m_Name = assemblyName;

        m_AssemblyInfos[assemblyName] = assemblyInfo;
        MonoClass* assetClass = ScriptAsset::GetMetaData()->ScriptClass;

        const Vector<MonoClass*>& allClasses = curAssembly->GetClasses();
        for (auto* klass : allClasses)
        {
            if (IsBasicType(klass))
                continue;
            const bool isSerializable = klass->IsSubClassOf(m_Builtin.Component) || klass->IsSubClassOf(assetClass) ||
                                        klass->HasAttribute(m_Builtin.SerializeObjectAttribute);
            if (klass->IsSubClassOf(m_Builtin.EntityBehaviour))
                m_EntityBehaviourClasses[klass->GetName()] = klass;
            const bool isInspectable = klass->HasAttribute(m_Builtin.ShowInInspectorAttribute);
            if ((isSerializable || isInspectable) && klass != m_Builtin.Component && klass != assetClass)
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

                // TODO: #ifdef these
                if (field->HasAttribute(m_Builtin.RangeAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::Range);
                /*if (field->HasAttribute(m_Builtin.StepAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::Step);*/
                if (field->HasAttribute(m_Builtin.NotNullAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::NotNull);
                if (field->HasAttribute(m_Builtin.DropdownAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::Dropdown);
                if (field->HasAttribute(m_Builtin.FilepathAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::Filepath);
                if (field->HasAttribute(m_Builtin.ReadOnlyAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::ReadOnly);
                if (field->HasAttribute(m_Builtin.MultilineAttribute))
                    fieldInfo->m_Flags.Set(ScriptFieldFlagBits::Multiline);

                if (field->HasAttribute(m_Builtin.ColorUsageAttribute))
                {
                    MonoObject* colorUsage = field->GetAttribute(m_Builtin.ColorUsageAttribute);
                    MonoField* showAlphaField = m_Builtin.ColorUsageAttribute->GetField("showAlpha");
                    MonoField* hdrField = m_Builtin.ColorUsageAttribute->GetField("hdr");

                    bool noAlpha = false;
                    showAlphaField->Get(colorUsage, &noAlpha);
                    bool hdr = false;
                    hdrField->Get(colorUsage, &hdr);

                    if (noAlpha)
                        fieldInfo->m_Flags.Set(ScriptFieldFlagBits::NoAlpha);
                    if (hdr)
                        fieldInfo->m_Flags.Set(ScriptFieldFlagBits::HDR);
                }

                // TODO: Also do of something better here. This is really bad.
                if (field->HasAttribute(m_Builtin.HeaderAttribute))
                {
                    MonoObject* label = field->GetAttribute(m_Builtin.HeaderAttribute);
                    MonoField* labelField = m_Builtin.HeaderAttribute->GetField("label");
                    MonoField* collapsableField = m_Builtin.HeaderAttribute->GetField("collapsable");
                    MonoString* stringValue = (MonoString*)labelField->GetBoxed(label);
                    String headerLabel = MonoUtils::FromMonoString(stringValue);
                    bool collapsable = false;
                    collapsableField->Get(label, &collapsable);
                    objInfo->m_Headers[fieldInfo->m_FieldId] = { headerLabel, collapsable };
                }
                // TODO: Do this in a better way. Maybe add some sort of additional info dictionary in the
                // fields/properties.
                if (field->HasAttribute(m_Builtin.LabelAttribute))
                {
                    MonoObject* label = field->GetAttribute(m_Builtin.LabelAttribute);
                    MonoField* labelField = m_Builtin.LabelAttribute->GetField("label");
                    MonoString* stringValue = (MonoString*)labelField->GetBoxed(label);
                    fieldInfo->m_Name = MonoUtils::FromMonoString(stringValue);
                }
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
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::Range);
                /*if (property->HasAttribute(m_Builtin.StepAttribute))
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::Step);*/
                if (property->HasAttribute(m_Builtin.NotNullAttribute))
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::NotNull);
                if (property->HasAttribute(m_Builtin.DropdownAttribute))
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::Dropdown);
                if (property->HasAttribute(m_Builtin.FilepathAttribute))
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::Filepath);
                if (property->HasAttribute(m_Builtin.ReadOnlyAttribute))
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::ReadOnly);
                if (property->HasAttribute(m_Builtin.MultilineAttribute))
                    propertyInfo->m_Flags.Set(ScriptFieldFlagBits::Multiline);

                if (property->HasAttribute(m_Builtin.ColorUsageAttribute))
                {
                    MonoObject* colorUsage = property->GetAttribute(m_Builtin.ColorUsageAttribute);
                    MonoField* showAlphaField = m_Builtin.ColorUsageAttribute->GetField("showAlpha");
                    MonoField* hdrField = m_Builtin.ColorUsageAttribute->GetField("hdr");

                    bool noAlpha = false;
                    showAlphaField->Get(colorUsage, &noAlpha);
                    bool hdr = false;
                    hdrField->Get(colorUsage, &hdr);

                    if (noAlpha)
                        propertyInfo->m_Flags.Set(ScriptFieldFlagBits::NoAlpha);
                    if (hdr)
                        propertyInfo->m_Flags.Set(ScriptFieldFlagBits::HDR);
                }

                // TODO: Do this in a better way. Maybe add some sort of additional info dictionary in the
                // fields/properties.
                if (property->HasAttribute(m_Builtin.LabelAttribute))
                {
                    MonoObject* label = property->GetAttribute(m_Builtin.LabelAttribute);
                    MonoField* labelField = m_Builtin.LabelAttribute->GetField("label");
                    MonoString* stringValue = nullptr;
                    labelField->Get(label, stringValue);
                    propertyInfo->m_Name = MonoUtils::FromMonoString(stringValue);
                }
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

    void ScriptInfoManager::RegisterAssets()
    {
        RegisterAsset<AudioClip, ScriptAudioClip>();
        RegisterAsset<Mesh, ScriptMesh>();
        // RegisterAsset<Texture, ScriptTexture>();
        // RegisterAsset<Shader, ScriptShader>();
    }

    void ScriptInfoManager::RegisterComponents()
    {
        RegisterComponent<TransformComponent, ScriptTransform>();
        RegisterComponent<CameraComponent, ScriptCamera>();
        // RegisterComponent<MonoScriptComponent, ScriptEntityBehaviour>(); // This should not be needed with the new
        // component system
        RegisterComponent<AudioSourceComponent, ScriptAudioSource>();
        RegisterComponent<AudioListenerComponent, ScriptAudioListener>();
        RegisterComponent<Rigidbody2DComponent, ScriptRigidbody2D>();

        RegisterComponent<Collider2D, ScriptCollider2D>();
        RegisterComponent<CircleCollider2DComponent, ScriptCircleCollider2D>();
        RegisterComponent<BoxCollider2DComponent, ScriptBoxCollider2D>();
    }

    Ref<SerializableTypeInfo> ScriptInfoManager::GetTypeInfo(MonoClass* monoClass)
    {
        CW_ENGINE_ASSERT(m_BaseTypesInitialized);
        MonoPrimitiveType primitiveType = MonoUtils::GetPrimitiveType(monoClass->GetInternalPtr());
        bool isEnum = MonoUtils::IsEnum(monoClass);
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
                // TODO: Do this in a better way. Comparing names is kinda stupid.
                Ref<SerializableTypeInfoPrimitive> typeInfo = CreateRef<SerializableTypeInfoPrimitive>();
                if (monoClass->GetFullName() == m_Builtin.Vector2->GetFullName())
                    scriptPrimitiveType = ScriptPrimitiveType::Vector2;
                else if (monoClass->GetFullName() == m_Builtin.Vector3->GetFullName())
                    scriptPrimitiveType = ScriptPrimitiveType::Vector3;
                else if (monoClass->GetFullName() == m_Builtin.Vector4->GetFullName())
                    scriptPrimitiveType = ScriptPrimitiveType::Vector4;
                else if (monoClass->GetFullName() == m_Builtin.Color->GetFullName())
                    scriptPrimitiveType = ScriptPrimitiveType::Color;
                else if (monoClass->GetFullName() == m_Builtin.Matrix4->GetFullName())
                    scriptPrimitiveType = ScriptPrimitiveType::Matrix4;
                typeInfo->m_Type = scriptPrimitiveType;
                return typeInfo;
            }
        }

        switch (primitiveType)
        {
        //    else if (monoClass->IsSubClassOf(m_Builtin.ComponentClass))
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
        case MonoPrimitiveType::Class: {
            if (monoClass->GetFullName() == m_Builtin.Entity->GetFullName()) // Entity
            {
                Ref<SerializableTypeInfoEntity> typeInfo = CreateRef<SerializableTypeInfoEntity>();
                typeInfo->m_TypeNamespace = monoClass->GetNamespace();
                typeInfo->m_TypeName = monoClass->GetName();
                return typeInfo;
            }
            else if (monoClass->IsSubClassOf(ScriptAsset::GetMetaData()->ScriptClass)) // Asset
            {
                Ref<SerializableTypeInfoAsset> typeInfo = CreateRef<SerializableTypeInfoAsset>();
                MonoReflectionType* reflType = MonoUtils::GetType(monoClass->GetInternalPtr());
                AssetInfo* assetInfo = GetAssetInfo(reflType);
                if (assetInfo == nullptr)
                {
                    CW_ENGINE_ERROR("Can't find asset type: {0}", monoClass->GetFullName());
                    return nullptr;
                }
                typeInfo->Type = assetInfo->Type;
                return typeInfo;
            }
            // Do components here
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
            break;
        case MonoPrimitiveType::Array: {
            Ref<SerializableTypeInfoArray> typeInfo = CreateRef<SerializableTypeInfoArray>();
            ::MonoClass* elementClass = ScriptArray::GetElementClassGlobal(monoClass->GetInternalPtr());
            if (elementClass != nullptr)
            {
                MonoClass* monoElementClass = MonoManager::Get().FindClass(elementClass);
                if (monoElementClass != nullptr)
                    typeInfo->m_ElementType = GetTypeInfo(monoElementClass);
            }
            if (typeInfo->m_ElementType == nullptr)
                return nullptr;
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
        m_AssetInfos.clear();
        m_AssemblyInfos.clear();
        m_EntityBehaviourClasses.clear();
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

    AssetInfo* ScriptInfoManager::GetAssetInfo(MonoReflectionType* type)
    {
        auto findIter = m_AssetInfos.find(type);
        if (findIter != m_AssetInfos.end())
            return &findIter->second;
        return nullptr;
    }

    AssetInfo* ScriptInfoManager::GetAssetInfo(AssetType typeId)
    {
        auto findIter = m_AssetInfosById.find((uint32_t)typeId);
        if (findIter != m_AssetInfosById.end())
            return &findIter->second;
        return nullptr;
    }

    AssetInfo* ScriptInfoManager::GetAssetInfo(uint32_t typeId)
    {
        auto findIter = m_AssetInfosById.find(typeId);
        if (findIter != m_AssetInfosById.end())
            return &findIter->second;
        return nullptr;
    }

} // namespace Crowny