#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"
#include "Editor/ScriptInspectorTransaction.h"
#include "Panels/ScriptInspectorModel.h"

using namespace Crowny;

namespace
{
    struct TestScriptState
    {
        PersistedScriptState State;
        Ref<SerializableMemberInfo> ValueField;
    };

    TestScriptState MakeScriptState(int32_t value)
    {
        const ScriptTypeIdentity identity{ "Missing.Assembly", "Missing.Namespace", "InspectorBehaviour" };

        Ref<SerializableTypeInfoObject> objectType = CreateRef<SerializableTypeInfoObject>();
        objectType->m_TypeNamespace = identity.Namespace;
        objectType->m_TypeName = identity.TypeName;
        objectType->m_TypeId = 1;
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
        Ref<SerializableFieldI32> data = CreateRef<SerializableFieldI32>();
        data->Value = value;
        object->SetFieldData(field, data);
        return { PersistedScriptState{ identity, object }, field };
    }

    Ref<SerializableFieldI32> GetValueField(Entity entity, const Ref<SerializableMemberInfo>& field)
    {
        const PersistedScriptState state = entity.GetComponent<MonoScriptComponent>().Scripts.front().CapturePersistedState();
        return StaticRefCast<SerializableFieldI32>(state.Fields->GetFieldData(field));
    }
} // namespace

TEST_CASE("Managed inspector edits retained state without constructing a script", "[Editor][Scripting][Inspector]")
{
    const TestScriptState retained = MakeScriptState(10);
    MonoScript script(retained.State.Identity);
    REQUIRE(script.ApplyPersistedState(retained.State));

    REQUIRE_FALSE(script.GetRuntimeHandle().IsValid());
    ScriptInspectorModel model(script);
    REQUIRE(model.GetState().Identity == retained.State.Identity);
    REQUIRE(model.GetState().Root.Members.at("Value") == ScriptValue::Signed(10));
    model.GetState().Root.Members.at("Value") = ScriptValue::Signed(20);
    REQUIRE(model.Commit());

    CHECK_FALSE(script.GetRuntimeHandle().IsValid());
    CHECK(script.GetManagedState().Root.Members.at("Value") == ScriptValue::Signed(20));
}

TEST_CASE("Managed inspector warm frames do not capture or allocate", "[Editor][Undo][Scripting][Memory]")
{
    constexpr size_t frameCount = 120u;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const TestScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script.State, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    UndoRedo::StartUp();

    bool allScopesStarted = true;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (size_t frameIndex = 0u; frameIndex < frameCount; frameIndex++)
    {
        allScopesStarted &= UndoRedo::Get().BeginComponentScope(transaction);
        transaction->CompleteFrame();
        UndoRedo::Get().EndComponentScope();
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);
    UndoRedo::Shutdown();

    CHECK(allScopesStarted);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}

TEST_CASE("Managed inspector captures redo state after the managed setter", "[Editor][Undo][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const TestScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script.State, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    Ref<SerializableFieldI32> liveValue = GetValueField(entity, script.ValueField);
    REQUIRE(liveValue != nullptr);

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 42u, false, false, false, true });
    liveValue->Value = 20;
    transaction->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    CHECK(UndoRedo::Get().GetUndoName() == "Edit script");
    UndoRedo::Get().Undo();
    CHECK(GetValueField(entity, script.ValueField)->Value == 10);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(GetValueField(entity, script.ValueField)->Value == 20);
    UndoRedo::Shutdown();
}

TEST_CASE("Managed inspector coalesces instantaneous changes from one frame", "[Editor][Undo][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const TestScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script.State, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    Ref<SerializableFieldI32> liveValue = GetValueField(entity, script.ValueField);
    REQUIRE(liveValue != nullptr);

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 42u, false, false, false, true });
    liveValue->Value = 20;
    UndoRedo::Get().OnItemInteract({ 43u, false, false, false, true });
    liveValue->Value = 30;
    transaction->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(GetValueField(entity, script.ValueField)->Value == 10);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(GetValueField(entity, script.ValueField)->Value == 30);
    UndoRedo::Shutdown();
}

TEST_CASE("Managed inspector finish and reset preserve redo state", "[Editor][Undo][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const TestScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script.State, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    Ref<SerializableFieldI32> liveValue = GetValueField(entity, script.ValueField);
    REQUIRE(liveValue != nullptr);

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 42u, true, true, false, true });
    liveValue->Value = 25;
    UndoRedo::Get().EndComponentScope();
    UndoRedo::Get().FinishComponentScope(transaction);
    transaction->Reset();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(GetValueField(entity, script.ValueField)->Value == 10);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(GetValueField(entity, script.ValueField)->Value == 25);
    UndoRedo::Shutdown();
}
