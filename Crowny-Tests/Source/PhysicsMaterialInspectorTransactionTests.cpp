#include <catch2/catch_test_macros.hpp>

#include "Editor/PhysicsMaterialInspectorTransaction.h"

using namespace Crowny;

namespace
{
    class UndoRedoScope
    {
    public:
        UndoRedoScope() { UndoRedo::StartUp(); }
        ~UndoRedoScope() { UndoRedo::Shutdown(); }
    };
} // namespace

TEST_CASE("Physics material drags produce one undo action with the final value", "[Editor][Assets][PhysicsMaterial][Undo]")
{
    const Ref<PhysicsMaterial2D> material = CreateRef<PhysicsMaterial2D>();
    const Ref<AssetSaveTracker> saves = CreateRef<AssetSaveTracker>();
    const Ref<PhysicsMaterialInspectorTransaction> transaction = CreateRef<PhysicsMaterialInspectorTransaction>();

    const UndoRedoScope undoRedoScope;
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    transaction->Capture("Assets/Surface.pmat", StaticRefCast<Asset>(material), saves);
    UndoRedo::Get().OnItemInteract({ 41u, true, true, false, true });
    material->SetFriction(0.7f);
    UndoRedo::Get().EndComponentScope();

    CHECK_FALSE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 41u, false, false, true, true });
    material->SetFriction(0.9f);
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    CHECK(UndoRedo::Get().GetUndoName() == "Edit physics material");
    UndoRedo::Get().Undo();
    CHECK(material->GetFriction() == 0.5f);
    CHECK_FALSE(UndoRedo::Get().CanUndo());

    const std::optional<AssetSaveRequest> undoSave = saves->TakeReady();
    REQUIRE(undoSave.has_value());
    CHECK(undoSave->Filepath == Path("Assets/Surface.pmat"));
    CHECK(undoSave->Value == StaticRefCast<Asset>(material));
    saves->Resolve(undoSave->Filepath, true);

    UndoRedo::Get().Redo();
    CHECK(material->GetFriction() == 0.9f);
    const std::optional<AssetSaveRequest> redoSave = saves->TakeReady();
    REQUIRE(redoSave.has_value());
    CHECK(redoSave->Filepath == Path("Assets/Surface.pmat"));
    saves->Resolve(redoSave->Filepath, true);
}

TEST_CASE("Discrete 3D physics material edits use the retained undo transaction", "[Editor][Assets][PhysicsMaterial][Undo]")
{
    const Ref<PhysicsMaterial3D> material = CreateRef<PhysicsMaterial3D>();
    const Ref<AssetSaveTracker> saves = CreateRef<AssetSaveTracker>();
    const Ref<PhysicsMaterialInspectorTransaction> transaction = CreateRef<PhysicsMaterialInspectorTransaction>();

    const UndoRedoScope undoRedoScope;
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    transaction->Capture("Assets/Surface.pmat3d", StaticRefCast<Asset>(material), saves);
    UndoRedo::Get().OnItemInteract({ 57u, false, false, false, true });
    material->SetRestitutionCombine(PhysicsCombineMode::Multiply);
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(material->GetRestitutionCombine() == PhysicsCombineMode::Maximum);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(material->GetRestitutionCombine() == PhysicsCombineMode::Multiply);
}
