#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/TextLayout.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scripting/Bindings/ScriptBindings.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/SceneComponentCodec.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Serialization/ScriptSerializer.h"

#include <mono/metadata/threads.h>

using namespace Crowny;

static_assert(std::is_nothrow_move_constructible_v<MonoScript>);
static_assert(std::is_nothrow_move_assignable_v<MonoScript>);

namespace
{
    constexpr TextLayoutFontData UNIT_LAYOUT_FONT{ 0.8, -0.2, 1.0, 1.0, 1.0, 4, nullptr };

    void AddLayoutToken(TextLayoutScratch& scratch, char32_t codePoint, bool whitespace = false, bool newLine = false)
    {
        TextLayoutToken& token = scratch.Tokens.Acquire();
        token = {};
        token.CodePoint = codePoint;
        token.Advance = newLine ? 0.0 : 1.0;
        token.NewLine = newLine;
        token.WhiteSpace = whitespace;
        token.BreakAfter = whitespace;
        token.Renderable = !whitespace && !newLine;
    }

    TextComponent MakeUnitTextComponent()
    {
        TextComponent component;
        component.Text = "prepared";
        component.Size = 36.0f;
        component.VerticalAlignment = TextVerticalAlignment::Baseline;
        return component;
    }

    struct RetainedScriptFields
    {
        Ref<SerializableObject> Object;
        Ref<SerializableMemberInfo> Field;
    };

    RetainedScriptFields MakeRetainedScriptFields(const ScriptTypeIdentity& identity, int32_t value)
    {
        Ref<SerializableTypeInfoObject> objectType = CreateRef<SerializableTypeInfoObject>();
        objectType->m_AssemblyName = identity.Assembly;
        objectType->m_TypeNamespace = identity.Namespace;
        objectType->m_TypeName = identity.TypeName;
        objectType->m_TypeId = 1;
        objectType->m_ValueType = false;
        objectType->m_Flags = ScriptFieldFlagBits::Serializable;

        Ref<SerializableObjectInfo> objectInfo = CreateRef<SerializableObjectInfo>();
        objectInfo->m_TypeInfo = objectType;

        Ref<SerializableTypeInfoPrimitive> fieldType = CreateRef<SerializableTypeInfoPrimitive>();
        fieldType->m_Type = ScriptPrimitiveType::I32;
        Ref<SerializableFieldInfo> field = CreateRef<SerializableFieldInfo>();
        field->m_Name = "Value";
        field->m_FieldId = 1;
        field->m_ParentTypeId = objectType->m_TypeId;
        field->m_TypeInfo = fieldType;
        field->m_Flags = ScriptFieldFlagBits::Serializable;
        objectInfo->m_Fields[field->m_FieldId] = field;
        objectInfo->m_FieldNameToId[field->m_Name] = field->m_FieldId;

        Ref<SerializableObject> object = CreateRef<SerializableObject>(objectInfo);
        Ref<SerializableFieldI32> fieldData = CreateRef<SerializableFieldI32>();
        fieldData->Value = value;
        object->SetFieldData(field, fieldData);
        return { object, field };
    }

    struct NestedTypeFields
    {
        Ref<SerializableObject> Object;
        Ref<SerializableMemberInfo> EnumField;
        Ref<SerializableMemberInfo> ObjectField;
    };

    NestedTypeFields MakeNestedTypeFields()
    {
        Ref<SerializableTypeInfoObject> rootType = CreateRef<SerializableTypeInfoObject>();
        rootType->m_AssemblyName = "Root.Managed";
        rootType->m_TypeNamespace = "Persisted";
        rootType->m_TypeName = "Container+Host";
        rootType->m_TypeId = 1;
        rootType->m_ValueType = false;
        rootType->m_Flags = ScriptFieldFlagBits::Serializable;

        Ref<SerializableObjectInfo> rootInfo = CreateRef<SerializableObjectInfo>();
        rootInfo->m_TypeInfo = rootType;

        Ref<SerializableTypeInfoEnum> enumType = CreateRef<SerializableTypeInfoEnum>();
        enumType->m_AssemblyName = "Enum.Managed";
        enumType->m_TypeNamespace = "Persisted";
        enumType->m_TypeName = "Container+Mode";
        enumType->m_UnderlyingType = ScriptPrimitiveType::I32;
        Ref<SerializableFieldInfo> enumField = CreateRef<SerializableFieldInfo>();
        enumField->m_Name = "Mode";
        enumField->m_FieldId = 1;
        enumField->m_ParentTypeId = rootType->m_TypeId;
        enumField->m_TypeInfo = enumType;
        enumField->m_Flags = ScriptFieldFlagBits::Serializable;

        Ref<SerializableTypeInfoObject> objectType = CreateRef<SerializableTypeInfoObject>();
        objectType->m_AssemblyName = "Object.Managed";
        objectType->m_TypeNamespace = "Persisted";
        objectType->m_TypeName = "Container+Payload";
        objectType->m_TypeId = 2;
        objectType->m_ValueType = false;
        objectType->m_Flags = ScriptFieldFlagBits::Serializable;
        Ref<SerializableFieldInfo> objectField = CreateRef<SerializableFieldInfo>();
        objectField->m_Name = "Payload";
        objectField->m_FieldId = 2;
        objectField->m_ParentTypeId = rootType->m_TypeId;
        objectField->m_TypeInfo = objectType;
        objectField->m_Flags = ScriptFieldFlagBits::Serializable;

        rootInfo->m_Fields.emplace(enumField->m_FieldId, enumField);
        rootInfo->m_Fields.emplace(objectField->m_FieldId, objectField);
        rootInfo->m_FieldNameToId.emplace(enumField->m_Name, enumField->m_FieldId);
        rootInfo->m_FieldNameToId.emplace(objectField->m_Name, objectField->m_FieldId);

        Ref<SerializableObject> object = CreateRef<SerializableObject>(rootInfo);
        Ref<SerializableFieldI32> enumData = CreateRef<SerializableFieldI32>();
        enumData->Value = 2;
        object->SetFieldData(enumField, enumData);
        Ref<SerializableFieldObject> objectData = CreateRef<SerializableFieldObject>();
        objectData->AllowNull = true;
        object->SetFieldData(objectField, objectData);
        return { object, enumField, objectField };
    }

    int32_t ReadRetainedScriptValue(const PersistedScriptState& state, const Ref<SerializableMemberInfo>& field)
    {
        if (state.Fields == nullptr)
            throw std::runtime_error("Persisted script fields are missing");
        Ref<SerializableFieldData> data = state.Fields->GetFieldData(field);
        if (data == nullptr)
            throw std::runtime_error("Persisted script field is missing");
        return StaticRefCast<SerializableFieldI32>(data)->Value;
    }

    float ReadRetainedScriptFloat(const PersistedScriptState& state, const Ref<SerializableMemberInfo>& field)
    {
        if (state.Fields == nullptr)
            throw std::runtime_error("Persisted script fields are missing");
        Ref<SerializableFieldData> data = state.Fields->GetFieldData(field);
        if (data == nullptr)
            throw std::runtime_error("Persisted script field is missing");
        return StaticRefCast<SerializableFieldFloat>(data)->Value;
    }

    Ref<SerializableObject> DetachScriptFields(const Ref<SerializableObject>& fields)
    {
        Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>();
        BinaryDataStreamOutputArchive output(stream);
        output(fields);
        stream->Seek(0);
        Ref<SerializableObject> detached;
        BinaryDataStreamInputArchive input(stream);
        input(detached);
        return detached;
    }

    void WriteLegacyBinaryScriptScene(const Path& path, const ScriptTypeIdentity& identity, const Ref<SerializableObject>& fields)
    {
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
        BinaryDataStreamOutputArchive archive(stream);
        archive(uint32_t{ 6 }, String("Legacy scripts"), String());
        archive(uint32_t{ 1 });
        archive(UuidGenerator::Generate(), String("Script host"), uint32_t{ 1 });
        archive(static_cast<uint32_t>(SceneComponentId::MonoScript));
        archive(uint32_t{ 1 }, identity.TypeName);
        {
            ScriptTypeMetadataSerializationScope legacyMetadata(false);
            Save(archive, *fields);
        }

        TimeSettings timeSettings;
        archive(timeSettings.TimeScale, timeSettings.MaxTimestep, timeSettings.FixedTimestep);
        Physics2DSettings physics2D;
        archive(physics2D.Gravity.x, physics2D.Gravity.y, physics2D.VelocityIterations, physics2D.PositionIterations);
        for (const String& layerName : physics2D.LayerNames)
            archive(layerName);
        for (uint32_t mask : physics2D.MaskBits)
            archive(mask);
        archive(UUID::EMPTY);
        Physics3DSettings physics3D;
        archive(static_cast<uint32_t>(physics3D.Backend));
        archive(physics3D.Gravity.x, physics3D.Gravity.y, physics3D.Gravity.z);
        archive(physics3D.Substeps, physics3D.EnableSleeping, physics3D.EnableContinuousCollision, physics3D.Deterministic);
        archive(UUID::EMPTY);
        stream->Close();
    }

    void WriteVersion7BinaryScriptScene(const Path& path, const ScriptTypeIdentity& identity, const Ref<SerializableObject>& fields)
    {
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
        BinaryDataStreamOutputArchive archive(stream);
        archive(uint32_t{ 7 }, String("Legacy nested metadata"), String());
        archive(uint32_t{ 1 });
        archive(UuidGenerator::Generate(), String("Script host"), uint32_t{ 1 });
        archive(static_cast<uint32_t>(SceneComponentId::MonoScript));
        archive(uint32_t{ 1 }, identity.Assembly, identity.Namespace, identity.TypeName, true);
        {
            ScriptTypeMetadataSerializationScope legacyMetadata(false);
            Save(archive, *fields);
        }

        TimeSettings timeSettings;
        archive(timeSettings.TimeScale, timeSettings.MaxTimestep, timeSettings.FixedTimestep);
        Physics2DSettings physics2D;
        archive(physics2D.Gravity.x, physics2D.Gravity.y, physics2D.VelocityIterations, physics2D.PositionIterations);
        for (const String& layerName : physics2D.LayerNames)
            archive(layerName);
        for (uint32_t mask : physics2D.MaskBits)
            archive(mask);
        archive(UUID::EMPTY);
        Physics3DSettings physics3D;
        archive(static_cast<uint32_t>(physics3D.Backend));
        archive(physics3D.Gravity.x, physics3D.Gravity.y, physics3D.Gravity.z);
        archive(physics3D.Substeps, physics3D.EnableSleeping, physics3D.EnableContinuousCollision, physics3D.Deterministic);
        archive(UUID::EMPTY);
        stream->Close();
    }

    void WriteVersion9BinaryTextScene(const Path& path)
    {
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
        BinaryDataStreamOutputArchive archive(stream);
        archive(uint32_t{ 9 }, String("Legacy text style"), String());
        archive(uint32_t{ 1 });
        archive(UuidGenerator::Generate(), String("LegacyText"), uint32_t{ 1 });
        archive(static_cast<uint32_t>(SceneComponentId::Text));

        archive(String("Old binary scene"), UUID::EMPTY);
        archive(1.0f, 1.0f, 1.0f, 1.0f);
        archive(36.0f, false, true);
        archive(0.0f, 0.0f, 0.0f, 0.0f);
        archive(0.8f, 0.0f, 0.0f, 0.0f, true);
        archive(uint32_t{ 0 }, static_cast<uint32_t>(TextOverflow::Overflow), static_cast<uint32_t>(TextHorizontalAlignment::Left),
                static_cast<uint32_t>(TextVerticalAlignment::Top));
        archive(8.0f, 72.0f, 0.0f, 0.0f);
        archive(static_cast<uint32_t>(TextWrapMode::WordThenCharacter), false, uint32_t{ 0 }, 0.0f);
        archive(false, 1.0f, 1.0f, 1.0f, 1.0f);
        archive(0.0f, 0.0f, 0.0f);
        archive(int32_t{ 0 }, int32_t{ 0 });

        TimeSettings timeSettings;
        archive(timeSettings.TimeScale, timeSettings.MaxTimestep, timeSettings.FixedTimestep);
        Physics2DSettings physics2D;
        archive(physics2D.Gravity.x, physics2D.Gravity.y, physics2D.VelocityIterations, physics2D.PositionIterations);
        for (const String& layerName : physics2D.LayerNames)
            archive(layerName);
        for (uint32_t mask : physics2D.MaskBits)
            archive(mask);
        archive(UUID::EMPTY);
        Physics3DSettings physics3D;
        archive(static_cast<uint32_t>(physics3D.Backend));
        archive(physics3D.Gravity.x, physics3D.Gravity.y, physics3D.Gravity.z);
        archive(physics3D.Substeps, physics3D.EnableSleeping, physics3D.EnableContinuousCollision, physics3D.Deterministic);
        archive(UUID::EMPTY);
        stream->Close();
    }
} // namespace

// Minimal fixture for scene serialization tests.
class SerializationTestFixture
{
public:
    SerializationTestFixture()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc desc;
            desc.Name = "Test";
            desc.Headless = true;
            desc.WorkingDirectory = fs::current_path();
            Application::StartUp(desc);
        }
    }

    ~SerializationTestFixture() {}
};

TEST_CASE("MonoScript vector moves preserve runtime identity", "[Scene][Scripting][Lifetime]")
{
    Vector<MonoScript> scripts;
    scripts.reserve(1);
    scripts.emplace_back(ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", "First" });
    const uint64_t firstId = scripts.front().InstanceId;

    scripts.emplace_back(ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", "Second" });
    const uint64_t secondId = scripts.back().InstanceId;
    CHECK(scripts.front().InstanceId == firstId);

    scripts.erase(scripts.begin());
    REQUIRE(scripts.size() == 1);
    CHECK(scripts.front().InstanceId == secondId);
}

TEST_CASE("MonoScript retains persisted state without a managed instance", "[Serialization][Scripting][PersistedState]")
{
    const ScriptTypeIdentity identity{ "Missing.Assembly", "Missing.Namespace", "MissingType" };
    const RetainedScriptFields retained = MakeRetainedScriptFields(identity, 37);
    const PersistedScriptState input{ identity, retained.Object };

    MonoScript script(identity);
    REQUIRE(script.ApplyPersistedState(input));
    const PersistedScriptState captured = script.CapturePersistedState();
    CHECK(captured.Identity == identity);
    CHECK(ReadRetainedScriptValue(captured, retained.Field) == 37);
    CHECK(script.GetManagedInstance() == nullptr);

    MonoScript copied(script);
    CHECK(copied.GetTypeIdentity() == identity);
    CHECK(ReadRetainedScriptValue(copied.CapturePersistedState(), retained.Field) == 37);
    MonoScript assigned;
    assigned = script;
    CHECK(assigned.GetTypeIdentity() == identity);
    CHECK(ReadRetainedScriptValue(assigned.CapturePersistedState(), retained.Field) == 37);
}

TEST_CASE("Missing managed scripts round-trip with exact identity and fields", "[Serialization][Scripting][PersistedState]")
{
    SerializationTestFixture fixture;
    const ScriptTypeIdentity identity{ "Unavailable.Assembly", "Preserved.Namespace", "GhostBehaviour" };
    const RetainedScriptFields retained = MakeRetainedScriptFields(identity, 91);
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("Missing scripts");
    Entity entity = scene->CreateEntity("Script host");
    REQUIRE(scene->AddScriptComponent(entity, PersistedScriptState{ identity, retained.Object }, false));

    const auto verify = [&](const Ref<Scene>& loaded) {
        const auto view = loaded->GetAllEntitiesWith<MonoScriptComponent>();
        REQUIRE(view.size() == 1);
        const MonoScriptComponent& component = view.get<MonoScriptComponent>(*view.begin());
        REQUIRE(component.Scripts.size() == 1);
        const PersistedScriptState state = component.Scripts.front().CapturePersistedState();
        CHECK(state.Identity == identity);
        CHECK(ReadRetainedScriptValue(state, retained.Field) == 91);
    };

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / "crowny-missing-script-state.yaml";
        SceneSerializer(scene).Serialize(path);
        const String yaml = FileSystem::OpenFile(path)->GetAsString();
        CHECK(yaml.find("Assembly: Unavailable.Assembly") != String::npos);
        CHECK(yaml.find("Namespace: Preserved.Namespace") != String::npos);
        CHECK(yaml.find("TypeName: GhostBehaviour") != String::npos);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).Deserialize(path));
        verify(loaded);
        fs::remove(path);
    }

    SECTION("Binary")
    {
        const Path path = fs::temp_directory_path() / "crowny-missing-script-state.cwb";
        SceneSerializer(scene).SerializeBinary(path);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).DeserializeBinary(path));
        verify(loaded);
        fs::remove(path);
    }
}

TEST_CASE("Retained script state applies when its managed type becomes available",
          "[Serialization][Scripting][PersistedState][Reload][.ProcessIsolated]")
{
    SerializationTestFixture fixture;
    if (!MonoManager::IsStartedUp())
    {
        const MonoRuntimePaths monoPaths = ResolveMonoRuntimePaths(fs::current_path());
        REQUIRE(monoPaths.HasRuntime());
        MonoManager::StartUp(monoPaths.LibraryDirectory, monoPaths.EtcDirectory, 0);
    }
    REQUIRE(MonoManager::Get().GetDomain() != nullptr);
    mono_thread_attach(MonoManager::Get().GetDomain());
    ScriptBindings::Register();
    if (!ScriptInfoManager::IsStartedUp())
        ScriptInfoManager::StartUp();
    if (!ScriptSceneObjectManager::IsStartedUp())
        ScriptSceneObjectManager::StartUp();
    if (!ScriptObjectManager::IsStartedUp())
        ScriptObjectManager::StartUp();
    if (!ScriptAssetManager::IsStartedUp())
        ScriptAssetManager::StartUp();

    const Path engineAssemblyPath = fs::absolute("Crowny-Sharp/CrownySharp.dll");
    const Path gameAssemblyPath = fs::absolute("Crowny-Sandbox/GameAssembly.dll");
    REQUIRE(fs::is_regular_file(engineAssemblyPath));
    REQUIRE(fs::is_regular_file(gameAssemblyPath));

    MonoManager::Get().LoadAssembly(engineAssemblyPath, CROWNY_ASSEMBLY);
    ScriptInfoManager::Get().InitializeTypes();
    ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
    MonoManager::Get().LoadAssembly(gameAssemblyPath, GAME_ASSEMBLY);
    ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);

    Ref<SerializableObjectInfo> nestedHostInfo;
    REQUIRE(ScriptInfoManager::Get().GetSerializableObjectInfo(GAME_ASSEMBLY, "Sandbox", "Test", nestedHostInfo));
    const auto enumFieldId = nestedHostInfo->m_FieldNameToId.find("dummyEnumInspector");
    REQUIRE(enumFieldId != nestedHostInfo->m_FieldNameToId.end());
    const Ref<SerializableTypeInfoEnum> nestedEnum =
      DynamicRefCast<SerializableTypeInfoEnum>(nestedHostInfo->m_Fields.at(enumFieldId->second)->m_TypeInfo);
    REQUIRE(nestedEnum != nullptr);
    CHECK(nestedEnum->m_AssemblyName == GAME_ASSEMBLY);
    CHECK(nestedEnum->m_TypeNamespace == "Sandbox");
    CHECK(nestedEnum->m_TypeName == "Test+DrawMode");

    const auto objectFieldId = nestedHostInfo->m_FieldNameToId.find("Struct");
    REQUIRE(objectFieldId != nestedHostInfo->m_FieldNameToId.end());
    const Ref<SerializableTypeInfoObject> nestedObject =
      DynamicRefCast<SerializableTypeInfoObject>(nestedHostInfo->m_Fields.at(objectFieldId->second)->m_TypeInfo);
    REQUIRE(nestedObject != nullptr);
    CHECK(nestedObject->m_AssemblyName == GAME_ASSEMBLY);
    CHECK(nestedObject->m_TypeNamespace == "Sandbox");
    CHECK(nestedObject->m_TypeName == "Test+TestStruct");

    Ref<SerializableObjectInfo> legacyNestedObject;
    REQUIRE(ScriptInfoManager::Get().GetSerializableObjectInfo("Sandbox", "TestStruct", legacyNestedObject));
    CHECK(legacyNestedObject->m_TypeInfo.get() == nestedObject.get());
    CHECK(legacyNestedObject->m_TypeInfo->m_AssemblyName == GAME_ASSEMBLY);
    CHECK(legacyNestedObject->m_TypeInfo->m_TypeName == "Test+TestStruct");

    const ScriptTypeIdentity identity{ GAME_ASSEMBLY, "Sandbox", "CameraFollow" };
    Ref<SerializableObjectInfo> initialInfo;
    REQUIRE(ScriptInfoManager::Get().GetSerializableObjectInfo(identity.Assembly, identity.Namespace, identity.TypeName, initialInfo));
    const auto initialFieldId = initialInfo->m_FieldNameToId.find("smoothSpeed");
    REQUIRE(initialFieldId != initialInfo->m_FieldNameToId.end());
    Ref<SerializableMemberInfo> initialField = initialInfo->m_Fields.at(initialFieldId->second);
    Ref<SerializableObject> fields = CreateRef<SerializableObject>(initialInfo);
    Ref<SerializableFieldFloat> value = CreateRef<SerializableFieldFloat>();
    value->Value = 7.25f;
    fields->SetFieldData(initialField, value);
    fields = DetachScriptFields(fields);

    Vector<AssemblyRefreshInfo> engineOnly;
    engineOnly.emplace_back(CROWNY_ASSEMBLY, &engineAssemblyPath);
    REQUIRE(ScriptObjectManager::Get().RefreshAssemblies(engineOnly).Succeeded());
    CHECK(MonoManager::Get().FindClass(identity.Assembly, identity.Namespace, identity.TypeName) == nullptr);

    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Reload host");
    REQUIRE(scene->AddScriptComponent(entity, PersistedScriptState{ identity, fields }));
    MonoScript& script = entity.GetComponent<MonoScriptComponent>().Scripts.front();
    CHECK(script.GetManagedClass() == nullptr);
    CHECK(script.CapturePersistedState().Fields != nullptr);

    Vector<AssemblyRefreshInfo> withGame;
    withGame.emplace_back(CROWNY_ASSEMBLY, &engineAssemblyPath);
    withGame.emplace_back(GAME_ASSEMBLY, &gameAssemblyPath);
    REQUIRE(ScriptObjectManager::Get().RefreshAssemblies(withGame).Succeeded());
    script.Create(entity);
    REQUIRE(script.GetManagedClass() != nullptr);
    REQUIRE(script.GetManagedInstance() != nullptr);

    Ref<SerializableObjectInfo> reloadedInfo;
    REQUIRE(ScriptInfoManager::Get().GetSerializableObjectInfo(identity.Assembly, identity.Namespace, identity.TypeName, reloadedInfo));
    const auto reloadedFieldId = reloadedInfo->m_FieldNameToId.find("smoothSpeed");
    REQUIRE(reloadedFieldId != reloadedInfo->m_FieldNameToId.end());
    Ref<SerializableMemberInfo> reloadedField = reloadedInfo->m_Fields.at(reloadedFieldId->second);
    const PersistedScriptState reloadedState = script.CapturePersistedState();
    CHECK(ReadRetainedScriptFloat(reloadedState, reloadedField) == Catch::Approx(7.25f));

    const auto targetFieldId = reloadedInfo->m_FieldNameToId.find("target");
    REQUIRE(targetFieldId != reloadedInfo->m_FieldNameToId.end());
    const Ref<SerializableMemberInfo> targetField = reloadedInfo->m_Fields.at(targetFieldId->second);
    const Ref<SerializableFieldEntity> target = StaticRefCast<SerializableFieldEntity>(reloadedState.Fields->GetFieldData(targetField));
    REQUIRE(target != nullptr);
    CHECK_FALSE(target->Value);
}

TEST_CASE("Legacy managed script entries still load", "[Serialization][Scripting][PersistedState][Legacy]")
{
    SerializationTestFixture fixture;
    const ScriptTypeIdentity legacyIdentity{ GAME_ASSEMBLY, "Sandbox", "LegacyBehaviour" };
    const RetainedScriptFields retained = MakeRetainedScriptFields({ "", legacyIdentity.Namespace, legacyIdentity.TypeName }, 123);

    const auto verify = [&](const Ref<Scene>& loaded) {
        const auto view = loaded->GetAllEntitiesWith<MonoScriptComponent>();
        REQUIRE(view.size() == 1);
        const MonoScriptComponent& component = view.get<MonoScriptComponent>(*view.begin());
        REQUIRE(component.Scripts.size() == 1);
        const PersistedScriptState state = component.Scripts.front().CapturePersistedState();
        CHECK(state.Identity == legacyIdentity);
        CHECK(ReadRetainedScriptValue(state, retained.Field) == 123);
    };

    SECTION("YAML mapping")
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", uint32_t{ 6 });
        SerializeValueYAML(out, "Scene", "Legacy scripts");
        SerializeValueYAML(out, "Entities", YAML::BeginSeq);
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Entity", UuidGenerator::Generate());
        BeginYAMLMap(out, "MonoScriptComponent");
        SerializeValueYAML(out, legacyIdentity.TypeName.c_str(), YAML::BeginSeq);
        retained.Object->SerializeYAML(out);
        EndYAMLSeq(out);
        EndYAMLMap(out, "MonoScriptComponent");
        out << YAML::EndMap;
        EndYAMLSeq(out);
        out << YAML::EndMap;

        const Path path = fs::temp_directory_path() / "crowny-legacy-script-state.yaml";
        const String legacyYaml(out.c_str(), out.size());
        CHECK(legacyYaml.find("Assembly:") == String::npos);
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
        stream->Write(legacyYaml.data(), legacyYaml.size());
        stream->Close();
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).Deserialize(path));
        verify(loaded);
        fs::remove(path);
    }

    SECTION("Binary type name")
    {
        const Path path = fs::temp_directory_path() / "crowny-legacy-script-state.cwb";
        WriteLegacyBinaryScriptScene(path, legacyIdentity, retained.Object);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).DeserializeBinary(path));
        verify(loaded);
        fs::remove(path);
    }
}

TEST_CASE("Nested managed type identities distinguish assemblies and accept legacy metadata", "[Serialization][Scripting][TypeIdentity]")
{
    Ref<SerializableTypeInfoObject> objectA = CreateRef<SerializableTypeInfoObject>();
    objectA->m_AssemblyName = "Assembly.A";
    objectA->m_TypeNamespace = "Shared";
    objectA->m_TypeName = "Container+Payload";
    Ref<SerializableTypeInfoObject> objectB = CreateRef<SerializableTypeInfoObject>();
    objectB->m_AssemblyName = "Assembly.B";
    objectB->m_TypeNamespace = objectA->m_TypeNamespace;
    objectB->m_TypeName = objectA->m_TypeName;
    Ref<SerializableTypeInfoObject> legacyObject = CreateRef<SerializableTypeInfoObject>();
    legacyObject->m_TypeNamespace = objectA->m_TypeNamespace;
    legacyObject->m_TypeName = "Payload";

    CHECK_FALSE(objectA->Matches(objectB));
    CHECK(objectA->Matches(legacyObject));

    Ref<SerializableTypeInfoEnum> enumA = CreateRef<SerializableTypeInfoEnum>();
    enumA->m_AssemblyName = "Assembly.A";
    enumA->m_TypeNamespace = "Shared";
    enumA->m_TypeName = "Container+Mode";
    enumA->m_UnderlyingType = ScriptPrimitiveType::I32;
    Ref<SerializableTypeInfoEnum> enumB = CreateRef<SerializableTypeInfoEnum>();
    enumB->m_AssemblyName = "Assembly.B";
    enumB->m_TypeNamespace = enumA->m_TypeNamespace;
    enumB->m_TypeName = enumA->m_TypeName;
    enumB->m_UnderlyingType = enumA->m_UnderlyingType;
    Ref<SerializableTypeInfoEnum> legacyEnum = CreateRef<SerializableTypeInfoEnum>();
    legacyEnum->m_TypeNamespace = enumA->m_TypeNamespace;
    legacyEnum->m_TypeName = "Mode";
    legacyEnum->m_UnderlyingType = enumA->m_UnderlyingType;

    CHECK_FALSE(enumA->Matches(enumB));
    CHECK(enumA->Matches(legacyEnum));
}

TEST_CASE("Nested managed type identities round-trip through scene formats", "[Serialization][Scripting][TypeIdentity]")
{
    SerializationTestFixture fixture;
    const ScriptTypeIdentity identity{ "Host.Managed", "Persisted", "MissingBehaviour" };
    const NestedTypeFields nested = MakeNestedTypeFields();
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("Nested managed metadata");
    Entity entity = scene->CreateEntity("Script host");
    REQUIRE(scene->AddScriptComponent(entity, PersistedScriptState{ identity, nested.Object }, false));

    const auto checkYamlIdentities = [](const String& yaml) {
        CHECK(yaml.find("Assembly: Root.Managed") != String::npos);
        CHECK(yaml.find("Assembly: Enum.Managed") != String::npos);
        CHECK(yaml.find("Assembly: Object.Managed") != String::npos);
    };

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / ("crowny-nested-type-" + UuidGenerator::Generate().ToString() + ".yaml");
        SceneSerializer(scene).Serialize(path);
        checkYamlIdentities(FileSystem::OpenFile(path)->GetAsString());

        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).Deserialize(path));
        const Path roundTripPath = path.string() + ".roundtrip";
        SceneSerializer(loaded).Serialize(roundTripPath);
        checkYamlIdentities(FileSystem::OpenFile(roundTripPath)->GetAsString());
        fs::remove(path);
        fs::remove(roundTripPath);
    }

    SECTION("Binary")
    {
        const Path path = fs::temp_directory_path() / ("crowny-nested-type-" + UuidGenerator::Generate().ToString() + ".cwb");
        SceneSerializer(scene).SerializeBinary(path);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).DeserializeBinary(path));

        const Path yamlPath = path.string() + ".yaml";
        SceneSerializer(loaded).Serialize(yamlPath);
        checkYamlIdentities(FileSystem::OpenFile(yamlPath)->GetAsString());
        fs::remove(path);
        fs::remove(yamlPath);
    }
}

TEST_CASE("Version 7 binary scenes load nested managed metadata", "[Serialization][Scripting][TypeIdentity][Legacy]")
{
    SerializationTestFixture fixture;
    const ScriptTypeIdentity identity{ "Host.Managed", "Persisted", "MissingBehaviour" };
    const NestedTypeFields nested = MakeNestedTypeFields();
    const Path path = fs::temp_directory_path() / ("crowny-v7-nested-type-" + UuidGenerator::Generate().ToString() + ".cwb");
    WriteVersion7BinaryScriptScene(path, identity, nested.Object);

    Ref<Scene> loaded = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(loaded).DeserializeBinary(path));
    const auto view = loaded->GetAllEntitiesWith<MonoScriptComponent>();
    REQUIRE(view.size() == 1);
    const MonoScriptComponent& component = view.get<MonoScriptComponent>(*view.begin());
    REQUIRE(component.Scripts.size() == 1);
    const PersistedScriptState state = component.Scripts.front().CapturePersistedState();
    REQUIRE(state.Identity == identity);
    REQUIRE(state.Fields != nullptr);
    const Ref<SerializableFieldData> enumData = state.Fields->GetFieldData(nested.EnumField);
    REQUIRE(enumData != nullptr);
    CHECK(StaticRefCast<SerializableFieldI32>(enumData)->Value == 2);
    CHECK(state.Fields->GetFieldData(nested.ObjectField) != nullptr);
    fs::remove(path);
}

TEST_CASE("Duplicate managed script identities are rejected", "[Scene][Scripting][PersistedState]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptTypeIdentity identity{ "Assembly", "Namespace", "Behaviour" };
    REQUIRE(scene->AddScriptComponent(entity, identity, false));
    CHECK_FALSE(scene->AddScriptComponent(entity, identity, false));
    REQUIRE(entity.HasComponent<MonoScriptComponent>());
    CHECK(entity.GetComponent<MonoScriptComponent>().Scripts.size() == 1);
    CHECK_FALSE(scene->AddScriptComponent(entity, ScriptTypeIdentity{ "", "Namespace", "Malformed" }, false));
}

TEST_CASE("Complex Scene Serialization", "[Serialization]")
{
    SerializationTestFixture fixture;

    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("ComplexScene");

    // Create a hierarchy:
    // Parent (10, 0, 0)
    //   +- Child1 (5, 0, 0) -> World (15, 0, 0)
    //   +- Child2 (0, 10, 0) -> World (10, 10, 0)
    //      +- GrandChild (0, 0, 5) -> World (10, 10, 5)

    Entity parent = scene->CreateEntity("Parent");
    parent.SetPosition({ 10.0f, 0.0f, 0.0f });
    auto& light = parent.AddComponent<LightComponent>();
    light.Type = LightType::Spot;
    light.Color = { 0.4f, 0.6f, 0.8f };
    light.Intensity = 2400.0f;
    light.Range = 31.0f;
    light.SpotInnerAngle = glm::radians(18.0f);
    light.SpotOuterAngle = glm::radians(37.0f);
    light.SourceRadius = 0.15f;
    light.UseColorTemperature = true;
    light.Temperature = 4200.0f;
    light.VisibilityLayers.Value = 0x00ff00ffu;
    light.AffectSpecular = false;
    light.Volumetric = true;
    light.Shadows.Mode = LightShadowMode::Soft;
    light.Shadows.Bias = 0.003f;
    light.Shadows.NormalBias = 0.04f;
    light.Shadows.NearPlane = 0.2f;
    light.Shadows.Importance = 2.5f;
    light.Shadows.Resolution = 2048;
    light.Shadows.CacheStaticCasters = false;

    Entity child1 = scene->CreateEntity("Child1");
    child1.SetParent(parent);
    child1.SetPosition({ 5.0f, 0.0f, 0.0f });

    auto& asc = child1.AddComponent<AudioSourceComponent>();
    asc.SetVolume(0.5f);
    asc.SetPitch(1.5f);
    asc.SetLooping(true);
    auto& rb2d = child1.AddComponent<Rigidbody2DComponent>();
    rb2d.SetAutoMass(false, child1);
    rb2d.SetCenterOfMass({ 0.25f, -0.5f });
    rb2d.SetInertia(2.75f);
    rb2d.SetLayerMask(99, child1);

    Entity child2 = scene->CreateEntity("Child2");
    child2.SetParent(parent);
    child2.SetPosition({ 0.0f, 10.0f, 0.0f });
    auto& rb3d = child2.AddComponent<Rigidbody3DComponent>();
    rb3d.SetBodyType(PhysicsBodyType3D::Dynamic, child2);
    rb3d.SetMass(3.5f, child2);
    rb3d.SetAutoMass(false, child2);
    rb3d.SetGravityScale(0.75f);
    rb3d.SetContinuousCollision(true, child2);
    auto& box3d = child2.AddComponent<BoxCollider3DComponent>();
    box3d.SetSize({ 2.0f, 3.0f, 4.0f }, child2);
    box3d.SetOffset({ 0.5f, 0.0f, -0.5f }, child2);
    Ref<PhysicsMaterial3D> material3d = CreateRef<PhysicsMaterial3D>();
    material3d->SetDensity(2.0f);
    material3d->SetFriction(0.25f);
    material3d->SetRestitution(0.6f);
    material3d->SetFrictionCombine(PhysicsCombineMode::Minimum);
    const AssetHandle<PhysicsMaterial3D> material3dHandle =
      static_asset_cast<PhysicsMaterial3D>(AssetManager::TryGet()->CreateAssetHandle(material3d));
    box3d.SetMaterial(material3dHandle);

    Entity grandChild = scene->CreateEntity("GrandChild");
    grandChild.SetParent(child2);
    grandChild.SetPosition({ 0.0f, 0.0f, 5.0f });
    auto& text = grandChild.AddComponent<TextComponent>();
    text.Text = "Aligned text";
    text.Size = 42.0f;
    text.AutoSize = true;
    text.AutoSizeMin = 12.0f;
    text.AutoSizeMax = 60.0f;
    text.LayoutSize = { 8.0f, 3.0f };
    text.Wrapping = true;
    text.WrapMode = TextWrapMode::WordThenCharacter;
    text.Overflow = TextOverflow::Ellipsis;
    text.ClipToBounds = true;
    text.MaxLines = 4;
    text.HorizontalAlignment = TextHorizontalAlignment::Justified;
    text.VerticalAlignment = TextVerticalAlignment::Middle;
    text.FontStyle = TextFontStyleBits::Underline | TextFontStyleBits::Strikethrough;
    text.CharacterSpacing = 0.05f;
    text.WordSpacing = 0.1f;
    text.LineSpacing = 0.2f;
    text.ParagraphSpacing = 0.35f;
    text.UseCustomDecorationColor = true;
    text.DecorationColor = { 0.2f, 0.4f, 0.6f, 0.8f };
    text.DecorationThickness = 0.03f;
    text.UnderlineOffset = -0.04f;
    text.StrikethroughOffset = 0.02f;
    text.SortingLayer = -3;
    text.OrderInLayer = 17;

    auto& sprite = child1.AddComponent<SpriteRendererComponent>();
    sprite.Color = { 0.25f, 0.5f, 0.75f, 1.0f };
    sprite.SortingLayer = 4;
    sprite.OrderInLayer = -9;

    Path testPath = "complex_scene.yaml";

    SECTION("Serialize and Deserialize Hierarchy and Components")
    {
        {
            SceneSerializer serializer(scene);
            serializer.Serialize(testPath);
        }

        Ref<Scene> deserializedScene = CreateRef<Scene>(false);
        {
            SceneSerializer serializer(deserializedScene);
            serializer.Deserialize(testPath);
        }

        Entity dParent = deserializedScene->FindEntityByName("Parent");
        Entity dChild1 = deserializedScene->FindEntityByName("Child1");
        Entity dChild2 = deserializedScene->FindEntityByName("Child2");
        Entity dGrandChild = deserializedScene->FindEntityByName("GrandChild");

        REQUIRE(dParent);
        REQUIRE(dChild1);
        REQUIRE(dChild2);
        REQUIRE(dGrandChild);

        CHECK(dChild1.GetParent() == dParent);
        CHECK(dChild2.GetParent() == dParent);
        CHECK(dGrandChild.GetParent() == dChild2);

        CHECK(dParent.GetWorldPosition() == glm::vec3(10.0f, 0.0f, 0.0f));
        CHECK(dChild1.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
        CHECK(dChild2.GetWorldPosition() == glm::vec3(10.0f, 10.0f, 0.0f));
        CHECK(dGrandChild.GetWorldPosition() == glm::vec3(10.0f, 10.0f, 5.0f));

        REQUIRE(dParent.HasComponent<LightComponent>());
        const LightComponent& dLight = dParent.GetComponent<LightComponent>();
        CHECK(dLight.Type == LightType::Spot);
        CHECK(dLight.Color == glm::vec3(0.4f, 0.6f, 0.8f));
        CHECK_THAT(dLight.Intensity, Catch::Matchers::WithinAbs(2400.0f, 0.0001f));
        CHECK_THAT(dLight.Range, Catch::Matchers::WithinAbs(31.0f, 0.0001f));
        CHECK_THAT(dLight.SpotInnerAngle, Catch::Matchers::WithinAbs(glm::radians(18.0f), 0.0001f));
        CHECK_THAT(dLight.SpotOuterAngle, Catch::Matchers::WithinAbs(glm::radians(37.0f), 0.0001f));
        CHECK_THAT(dLight.SourceRadius, Catch::Matchers::WithinAbs(0.15f, 0.0001f));
        CHECK(dLight.UseColorTemperature);
        CHECK_THAT(dLight.Temperature, Catch::Matchers::WithinAbs(4200.0f, 0.0001f));
        CHECK(dLight.VisibilityLayers.Value == 0x00ff00ffu);
        CHECK_FALSE(dLight.AffectSpecular);
        CHECK(dLight.Volumetric);
        CHECK(dLight.Shadows.Mode == LightShadowMode::Soft);
        CHECK_THAT(dLight.Shadows.Bias, Catch::Matchers::WithinAbs(0.003f, 0.0001f));
        CHECK_THAT(dLight.Shadows.NormalBias, Catch::Matchers::WithinAbs(0.04f, 0.0001f));
        CHECK_THAT(dLight.Shadows.NearPlane, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
        CHECK_THAT(dLight.Shadows.Importance, Catch::Matchers::WithinAbs(2.5f, 0.0001f));
        CHECK(dLight.Shadows.Resolution == 2048);
        CHECK_FALSE(dLight.Shadows.CacheStaticCasters);

        REQUIRE(dGrandChild.HasComponent<TextComponent>());
        const auto& dText = dGrandChild.GetComponent<TextComponent>();
        CHECK(dText.Text == "Aligned text");
        CHECK_THAT(dText.Size, Catch::Matchers::WithinAbs(42.0f, 0.0001f));
        CHECK(dText.AutoSize);
        CHECK_THAT(dText.AutoSizeMin, Catch::Matchers::WithinAbs(12.0f, 0.0001f));
        CHECK_THAT(dText.AutoSizeMax, Catch::Matchers::WithinAbs(60.0f, 0.0001f));
        CHECK(dText.LayoutSize == glm::vec2(8.0f, 3.0f));
        CHECK(dText.WrapMode == TextWrapMode::WordThenCharacter);
        CHECK(dText.Overflow == TextOverflow::Ellipsis);
        CHECK(dText.ClipToBounds);
        CHECK(dText.MaxLines == 4);
        CHECK(dText.HorizontalAlignment == TextHorizontalAlignment::Justified);
        CHECK(dText.VerticalAlignment == TextVerticalAlignment::Middle);
        CHECK(dText.FontStyle.IsSet(TextFontStyleBits::Underline));
        CHECK(dText.FontStyle.IsSet(TextFontStyleBits::Strikethrough));
        CHECK_THAT(dText.CharacterSpacing, Catch::Matchers::WithinAbs(0.05f, 0.0001f));
        CHECK_THAT(dText.WordSpacing, Catch::Matchers::WithinAbs(0.1f, 0.0001f));
        CHECK_THAT(dText.LineSpacing, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
        CHECK_THAT(dText.ParagraphSpacing, Catch::Matchers::WithinAbs(0.35f, 0.0001f));
        CHECK(dText.UseCustomDecorationColor);
        CHECK(dText.DecorationColor == glm::vec4(0.2f, 0.4f, 0.6f, 0.8f));
        CHECK_THAT(dText.DecorationThickness, Catch::Matchers::WithinAbs(0.03f, 0.0001f));
        CHECK_THAT(dText.UnderlineOffset, Catch::Matchers::WithinAbs(-0.04f, 0.0001f));
        CHECK_THAT(dText.StrikethroughOffset, Catch::Matchers::WithinAbs(0.02f, 0.0001f));
        CHECK(dText.SortingLayer == -3);
        CHECK(dText.OrderInLayer == 17);

        REQUIRE(dChild1.HasComponent<SpriteRendererComponent>());
        const auto& dSprite = dChild1.GetComponent<SpriteRendererComponent>();
        CHECK(dSprite.Color == glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
        CHECK(dSprite.SortingLayer == 4);
        CHECK(dSprite.OrderInLayer == -9);

        REQUIRE(dChild2.HasComponent<Rigidbody3DComponent>());
        const auto& dRb3d = dChild2.GetComponent<Rigidbody3DComponent>();
        CHECK(dRb3d.GetBodyType() == PhysicsBodyType3D::Dynamic);
        CHECK_THAT(dRb3d.GetMass(), Catch::Matchers::WithinAbs(3.5f, 0.0001f));
        CHECK_FALSE(dRb3d.GetAutoMass());
        CHECK(dRb3d.GetContinuousCollision());
        REQUIRE(dChild2.HasComponent<BoxCollider3DComponent>());
        const auto& dBox3d = dChild2.GetComponent<BoxCollider3DComponent>();
        CHECK(dBox3d.GetSize() == glm::vec3(2.0f, 3.0f, 4.0f));
        CHECK(dBox3d.GetOffset() == glm::vec3(0.5f, 0.0f, -0.5f));
        REQUIRE(dBox3d.GetMaterial());
        CHECK_THAT(dBox3d.GetMaterial()->GetRestitution(), Catch::Matchers::WithinAbs(0.6f, 0.0001f));
        CHECK(dBox3d.GetMaterial()->GetFrictionCombine() == PhysicsCombineMode::Minimum);

        REQUIRE(dChild1.HasComponent<AudioSourceComponent>());
        auto& dAsc = dChild1.GetComponent<AudioSourceComponent>();
        CHECK(dAsc.GetVolume() == 0.5f);
        CHECK(dAsc.GetPitch() == 1.5f);
        CHECK(dAsc.GetLooping() == true);

        REQUIRE(dChild1.HasComponent<Rigidbody2DComponent>());
        const auto& dRb2d = dChild1.GetComponent<Rigidbody2DComponent>();
        CHECK(dRb2d.GetConfiguredCenterOfMass() == glm::vec2(0.25f, -0.5f));
        CHECK_THAT(dRb2d.GetConfiguredInertia(), Catch::Matchers::WithinAbs(2.75f, 0.0001f));
        CHECK(dRb2d.GetLayerMask() == Physics2DLayerCount - 1);
    }
}

TEST_CASE("Physics material references survive YAML and binary scene round trips", "[Serialization][Physics]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("PhysicsMaterials");
    Entity entity = scene->CreateEntity("Colliders");

    const AssetHandle<PhysicsMaterial2D> material2DHandle = CreateRuntimePhysicsMaterial2D(*AssetManager::TryGet());
    material2DHandle->SetFriction(0.15f);
    material2DHandle->SetFrictionCombine(PhysicsCombineMode::Maximum);
    entity.AddComponent<BoxCollider2DComponent>().SetMaterial(material2DHandle);

    const AssetHandle<PhysicsMaterial3D> material3DHandle = CreateRuntimePhysicsMaterial3D(*AssetManager::TryGet());
    material3DHandle->SetRestitution(0.85f);
    material3DHandle->SetRestitutionCombine(PhysicsCombineMode::Multiply);
    entity.AddComponent<SphereCollider3DComponent>().SetMaterial(material3DHandle);

    const auto verify = [&](const Ref<Scene>& loadedScene) {
        Entity loadedEntity = loadedScene->FindEntityByName("Colliders");
        REQUIRE(loadedEntity);
        const auto& collider2D = loadedEntity.GetComponent<BoxCollider2DComponent>();
        const auto& collider3D = loadedEntity.GetComponent<SphereCollider3DComponent>();
        REQUIRE(collider2D.GetMaterial());
        REQUIRE(collider3D.GetMaterial());
        CHECK(collider2D.GetMaterial()->GetFriction() == 0.15f);
        CHECK(collider3D.GetMaterial()->GetRestitution() == 0.85f);
        CHECK(collider2D.GetMaterial()->GetFrictionCombine() == PhysicsCombineMode::Maximum);
        CHECK(collider3D.GetMaterial()->GetRestitutionCombine() == PhysicsCombineMode::Multiply);
        CHECK(collider2D.GetMaterial().GetUUID() != material2DHandle.GetUUID());
        CHECK(collider3D.GetMaterial().GetUUID() != material3DHandle.GetUUID());
    };

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / "crowny-physics-material-scene.yaml";
        SceneSerializer(scene).Serialize(path);
        Ref<Scene> loadedScene = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loadedScene).Deserialize(path));
        verify(loadedScene);
        fs::remove(path);
    }

    SECTION("Binary")
    {
        const Path path = fs::temp_directory_path() / "crowny-physics-material-scene.cwb";
        SceneSerializer(scene).SerializeBinary(path);
        Ref<Scene> loadedScene = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loadedScene).DeserializeBinary(path));
        verify(loadedScene);
        fs::remove(path);
    }
}

TEST_CASE("Missing physics material assets retain their references and use defaults", "[Serialization][Physics]")
{
    SerializationTestFixture fixture;
    const UUID missing2D = UuidGenerator::Generate();
    const UUID missing3D = UuidGenerator::Generate();

    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("MissingMaterials");
    Entity entity = scene->CreateEntity("MissingColliders");
    auto& collider2D = entity.AddComponent<BoxCollider2DComponent>();
    collider2D.SetMaterial(static_asset_cast<PhysicsMaterial2D>(AssetManager::TryGet()->GetAssetHandle(missing2D)));
    auto& collider3D = entity.AddComponent<BoxCollider3DComponent>();
    collider3D.SetMaterial(static_asset_cast<PhysicsMaterial3D>(AssetManager::TryGet()->GetAssetHandle(missing3D)));

    const Path path = fs::temp_directory_path() / "crowny-missing-physics-material-scene.yaml";
    SceneSerializer(scene).Serialize(path);
    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(loadedScene).Deserialize(path));
    Entity loadedEntity = loadedScene->FindEntityByName("MissingColliders");
    REQUIRE(loadedEntity);
    const auto& loaded2D = loadedEntity.GetComponent<BoxCollider2DComponent>();
    const auto& loaded3D = loadedEntity.GetComponent<BoxCollider3DComponent>();
    CHECK_FALSE(loaded2D.GetMaterial().IsLoaded());
    CHECK_FALSE(loaded3D.GetMaterial().IsLoaded());
    CHECK(loaded2D.GetMaterial().GetUUID() == missing2D);
    CHECK(loaded3D.GetMaterial().GetUUID() == missing3D);
    CHECK(loaded2D.GetMaterialData().Density == Physics2D::TryGet()->GetDefaultMaterial()->GetDensity());
    CHECK(loaded3D.GetMaterialData().Density == Physics3D::Get().GetDefaultMaterial()->GetDensity());
    fs::remove(path);
}

TEST_CASE("Failed YAML scene loads discard partial entities", "[Serialization]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->CreateEntity("Existing");

    YAML::Emitter emitter;
    emitter << YAML::BeginMap << YAML::Key << "Scene" << YAML::Value << "Broken" << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq
            << YAML::BeginMap << YAML::Key << "Entity" << YAML::Value << UuidGenerator::Generate() << YAML::Key << "TagComponent" << YAML::Value
            << YAML::BeginMap << YAML::Key << "Tag" << YAML::Value << "Partial" << YAML::EndMap << YAML::Key << "BoxCollider2DComponent"
            << YAML::Value << YAML::BeginMap << YAML::Key << "Offset" << YAML::Value << "invalid" << YAML::Key << "Size" << YAML::Value
            << glm::vec2(1.0f) << YAML::Key << "IsTrigger" << YAML::Value << false << YAML::EndMap << YAML::EndMap << YAML::EndSeq << YAML::EndMap;

    const Path path = fs::temp_directory_path() / "crowny-broken-scene.yaml";
    const Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
    REQUIRE(stream != nullptr);
    stream->Write(emitter.c_str(), emitter.size());
    stream->Close();

    CHECK_FALSE(SceneSerializer(scene).Deserialize(path));
    CHECK(scene->GetRootEntity());
    CHECK_FALSE(scene->FindEntityByName("Partial"));
    CHECK_FALSE(scene->FindEntityByName("Existing"));
    fs::remove(path);
}

TEST_CASE("Light binary serialization", "[Serialization][Lights]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("LightScene");
    Entity entity = scene->CreateEntity("Sun");
    auto& source = entity.AddComponent<LightComponent>();
    source.Type = LightType::Directional;
    source.Intensity = 110000.0f;
    source.Color = { 1.0f, 0.92f, 0.81f };
    source.VisibilityLayers.Value = 0x5u;
    source.Shadows.Mode = LightShadowMode::Soft;
    source.Shadows.Bias = 0.0005f;
    source.Shadows.NormalBias = 0.025f;
    source.Shadows.Resolution = 4096;

    const Path path = "light_scene.cwb";
    SceneSerializer(scene).SerializeBinary(path);

    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    SceneSerializer(loadedScene).DeserializeBinary(path);
    Entity loadedEntity = loadedScene->FindEntityByName("Sun");
    REQUIRE(loadedEntity);
    REQUIRE(loadedEntity.HasComponent<LightComponent>());
    const LightComponent& loaded = loadedEntity.GetComponent<LightComponent>();
    CHECK(loaded.Type == LightType::Directional);
    CHECK_THAT(loaded.Intensity, Catch::Matchers::WithinAbs(110000.0f, 0.001f));
    CHECK(loaded.Color == source.Color);
    CHECK(loaded.VisibilityLayers.Value == 0x5u);
    CHECK(loaded.Shadows.Mode == LightShadowMode::Soft);
    CHECK_THAT(loaded.Shadows.Bias, Catch::Matchers::WithinAbs(0.0005f, 0.000001f));
    CHECK_THAT(loaded.Shadows.NormalBias, Catch::Matchers::WithinAbs(0.025f, 0.000001f));
    CHECK(loaded.Shadows.Resolution == 4096);
}

TEST_CASE("Text layout binary serialization", "[Serialization][Text]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("TextLayoutScene");
    Entity entity = scene->CreateEntity("Text");
    auto& text = entity.AddComponent<TextComponent>();
    const UUID fontId = UuidGenerator::Generate();
    text.Font = static_asset_cast<Font>(AssetManager::TryGet()->CreateAssetHandle(CreateRef<Font>(), fontId));
    text.Text = "First paragraph\nSecond paragraph";
    text.AutoSize = true;
    text.AutoSizeMin = 10.0f;
    text.AutoSizeMax = 54.0f;
    text.LayoutSize = { 7.5f, 4.25f };
    text.WrapMode = TextWrapMode::Character;
    text.Overflow = TextOverflow::Truncate;
    text.ClipToBounds = true;
    text.MaxLines = 3;
    text.TabWidth = 7;
    text.ParagraphSpacing = 0.75f;
    text.ShadowColor = { 0.1f, 0.2f, 0.3f, 0.65f };
    text.ShadowOffset = { 0.3f, -0.45f };
    text.ShadowSoftness = 0.2f;
    text.UseCustomDecorationColor = true;
    text.DecorationColor = { 0.8f, 0.6f, 0.4f, 0.2f };
    text.DecorationThickness = 0.08f;
    text.UnderlineOffset = -0.1f;
    text.StrikethroughOffset = 0.15f;
    text.SortingLayer = 12;
    text.OrderInLayer = -7;

    const Path path = "text_layout_scene.cwb";
    SceneSerializer(scene).SerializeBinary(path);

    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    SceneSerializer(loadedScene).DeserializeBinary(path);
    Entity loadedEntity = loadedScene->FindEntityByName("Text");
    REQUIRE(loadedEntity);
    REQUIRE(loadedEntity.HasComponent<TextComponent>());

    const auto& loaded = loadedEntity.GetComponent<TextComponent>();
    CHECK(loaded.Text == text.Text);
    CHECK(loaded.Font.GetUUID() == fontId);
    CHECK(loaded.AutoSize);
    CHECK_THAT(loaded.AutoSizeMin, Catch::Matchers::WithinAbs(10.0f, 0.0001f));
    CHECK_THAT(loaded.AutoSizeMax, Catch::Matchers::WithinAbs(54.0f, 0.0001f));
    CHECK(loaded.LayoutSize == glm::vec2(7.5f, 4.25f));
    CHECK(loaded.WrapMode == TextWrapMode::Character);
    CHECK(loaded.Overflow == TextOverflow::Truncate);
    CHECK(loaded.ClipToBounds);
    CHECK(loaded.MaxLines == 3);
    CHECK(loaded.TabWidth == 7);
    CHECK_THAT(loaded.ParagraphSpacing, Catch::Matchers::WithinAbs(0.75f, 0.0001f));
    CHECK(loaded.ShadowColor == glm::vec4(0.1f, 0.2f, 0.3f, 0.65f));
    CHECK(loaded.ShadowOffset == glm::vec2(0.3f, -0.45f));
    CHECK_THAT(loaded.ShadowSoftness, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
    CHECK(loaded.UseCustomDecorationColor);
    CHECK(loaded.DecorationColor == glm::vec4(0.8f, 0.6f, 0.4f, 0.2f));
    CHECK_THAT(loaded.DecorationThickness, Catch::Matchers::WithinAbs(0.08f, 0.0001f));
    CHECK_THAT(loaded.UnderlineOffset, Catch::Matchers::WithinAbs(-0.1f, 0.0001f));
    CHECK_THAT(loaded.StrikethroughOffset, Catch::Matchers::WithinAbs(0.15f, 0.0001f));
    CHECK(loaded.SortingLayer == 12);
    CHECK(loaded.OrderInLayer == -7);
}

TEST_CASE("Legacy text scenes use stable tab and shadow defaults", "[Serialization][Text][Legacy]")
{
    SerializationTestFixture fixture;
    const Path path = fs::temp_directory_path() / "crowny-legacy-text-style.yaml";
    const UUID entityId;

    YAML::Emitter out;
    out << YAML::BeginMap;
    SerializeValueYAML(out, "Version", uint32_t{ 9 });
    SerializeValueYAML(out, "Scene", "LegacyTextStyle");
    SerializeValueYAML(out, "Entities", YAML::BeginSeq);
    out << YAML::BeginMap;
    SerializeValueYAML(out, "Entity", entityId);
    BeginYAMLMap(out, "TagComponent");
    SerializeValueYAML(out, "Tag", "LegacyText");
    EndYAMLMap(out, "TagComponent");
    BeginYAMLMap(out, "TextComponent");
    SerializeValueYAML(out, "Text", "Old scene");
    EndYAMLMap(out, "TextComponent");
    out << YAML::EndMap << YAML::EndSeq << YAML::EndMap;

    Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
    stream->Write(out.c_str(), std::strlen(out.c_str()));
    stream->Close();

    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(loadedScene).Deserialize(path));
    Entity loadedEntity = loadedScene->FindEntityByName("LegacyText");
    REQUIRE(loadedEntity);
    REQUIRE(loadedEntity.HasComponent<TextComponent>());

    const TextComponent& loaded = loadedEntity.GetComponent<TextComponent>();
    CHECK(loaded.TabWidth == 4);
    CHECK(loaded.ShadowColor == glm::vec4(0.0f));
    CHECK(loaded.ShadowOffset == glm::vec2(1.0f, -1.0f));
    CHECK_THAT(loaded.ShadowSoftness, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    fs::remove(path);
}

TEST_CASE("Version 9 binary text scenes use stable tab and shadow defaults", "[Serialization][Text][Legacy]")
{
    SerializationTestFixture fixture;
    const Path path = fs::temp_directory_path() / "crowny-version-9-text-style.cwb";
    WriteVersion9BinaryTextScene(path);

    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(loadedScene).DeserializeBinary(path));
    Entity loadedEntity = loadedScene->FindEntityByName("LegacyText");
    REQUIRE(loadedEntity);
    REQUIRE(loadedEntity.HasComponent<TextComponent>());

    const TextComponent& loaded = loadedEntity.GetComponent<TextComponent>();
    CHECK(loaded.Text == "Old binary scene");
    CHECK(loaded.TabWidth == 4);
    CHECK(loaded.ShadowColor == glm::vec4(0.0f));
    CHECK(loaded.ShadowOffset == glm::vec2(1.0f, -1.0f));
    CHECK_THAT(loaded.ShadowSoftness, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    fs::remove(path);
}

TEST_CASE("Text layout wrapping modes handle oversized words", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 2.0f, 0.0f };
    TextLayoutScratch scratch;
    for (uint32_t i = 0; i < 4; i++)
        AddLayoutToken(scratch, U'a');

    component.WrapMode = TextWrapMode::Word;
    TextLayoutResult word = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK(word.LineCount == 1);
    CHECK(word.OverflowedHorizontally);

    component.WrapMode = TextWrapMode::Character;
    TextLayoutResult character = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(character.LineCount == 2);
    CHECK_THAT(character.Lines[0].Width, Catch::Matchers::WithinAbs(2.0f, 0.0001f));
    CHECK_THAT(character.Lines[1].Width, Catch::Matchers::WithinAbs(2.0f, 0.0001f));

    component.WrapMode = TextWrapMode::WordThenCharacter;
    TextLayoutResult fallback = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK(fallback.LineCount == 2);
}

TEST_CASE("Text layout positions bounded alignment and flush spacing", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 10.0f, 10.0f };
    component.Wrapping = false;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U' ', true);
    AddLayoutToken(scratch, U'b');

    component.HorizontalAlignment = TextHorizontalAlignment::Center;
    TextLayoutResult center = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(center.LineCount == 1);
    CHECK_THAT(center.Lines[0].X, Catch::Matchers::WithinAbs(3.5f, 0.0001f));

    component.HorizontalAlignment = TextHorizontalAlignment::Right;
    TextLayoutResult right = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK_THAT(right.Lines[0].X, Catch::Matchers::WithinAbs(7.0f, 0.0001f));

    component.HorizontalAlignment = TextHorizontalAlignment::Flush;
    TextLayoutResult flush = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(flush.GlyphCount == 2);
    CHECK_THAT(flush.Lines[0].Width, Catch::Matchers::WithinAbs(10.0f, 0.0001f));
    CHECK_THAT(flush.Glyphs[0].PenPosition.x, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    CHECK_THAT(flush.Glyphs[1].PenPosition.x, Catch::Matchers::WithinAbs(9.0f, 0.0001f));

    component.VerticalAlignment = TextVerticalAlignment::Bottom;
    TextLayoutResult bottom = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK_THAT(bottom.Lines[0].Baseline, Catch::Matchers::WithinAbs(-9.8f, 0.0001f));
}

TEST_CASE("Text layout ellipsizes hidden lines and fits font size", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 10.0f, 1.0f };
    component.Overflow = TextOverflow::Ellipsis;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U'\n', false, true);
    AddLayoutToken(scratch, U'b');

    TextLayoutResult ellipsis = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(ellipsis.LineCount == 1);
    REQUIRE(ellipsis.GlyphCount == 2);
    CHECK(ellipsis.Truncated);
    CHECK(ellipsis.Lines[0].Ellipsized);
    CHECK(ellipsis.Glyphs[1].CodePoint == 0x2026);

    TextLayoutScratch autosizeScratch;
    AddLayoutToken(autosizeScratch, U'a');
    component.LayoutSize = { 2.0f, 2.0f };
    component.Overflow = TextOverflow::Overflow;
    component.AutoSize = true;
    component.AutoSizeMin = 8.0f;
    component.AutoSizeMax = 72.0f;
    TextLayoutResult autosized = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, autosizeScratch);
    CHECK_THAT(autosized.FontSize, Catch::Matchers::WithinAbs(72.0f, 0.05f));
}

TEST_CASE("Scene component codecs have stable complete registrations", "[Serialization][Scene]")
{
    const auto codecs = GetSceneComponentCodecs();
    REQUIRE(codecs.size() == 21);

    for (size_t index = 0; index < codecs.size(); index++)
    {
        const SceneComponentCodec& codec = codecs[index];
        CHECK(static_cast<uint32_t>(codec.Id) == index);
        CHECK(codec.Version > 0);
        CHECK(codec.YamlName != nullptr);
        CHECK(codec.HasComponent != nullptr);
        CHECK(codec.ShouldSerialize != nullptr);
        CHECK(codec.WriteYaml != nullptr);
        CHECK(codec.ReadYaml != nullptr);
        CHECK(codec.WriteBinary != nullptr);
        CHECK(codec.ReadBinary != nullptr);
        CHECK(codec.Migrate != nullptr);
        CHECK(FindSceneComponentCodec(static_cast<uint32_t>(codec.Id)) == &codec);

        for (size_t otherIndex = index + 1; otherIndex < codecs.size(); otherIndex++)
            CHECK(StringView(codec.YamlName) != StringView(codecs[otherIndex].YamlName));
    }

    CHECK(static_cast<uint32_t>(SceneComponentId::Tag) == 0);
    CHECK(static_cast<uint32_t>(SceneComponentId::Light) == 20);
    CHECK(FindSceneComponentCodec(21) == nullptr);
    CHECK(FindSceneComponentCodec(SceneComponentId::Transform)->PrefabPath != nullptr);
    CHECK(FindSceneComponentCodec(SceneComponentId::Transform)->EditorName != nullptr);
}
