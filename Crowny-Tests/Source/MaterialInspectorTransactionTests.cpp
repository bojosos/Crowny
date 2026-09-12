#include <catch2/catch_test_macros.hpp>

#include "Editor/MaterialInspectorTransaction.h"

using namespace Crowny;

namespace
{
    struct MaterialUndoScope
    {
        MaterialUndoScope()
        {
            AssetListenerManager::StartUp();
            UndoRedo::StartUp();
            MaterialAsset = Material::Create({});
        }
        ~MaterialUndoScope()
        {
            UndoRedo::Shutdown();
            Transaction->Reset();
            MaterialAsset = nullptr;
            Saves = nullptr;
            AssetListenerManager::Shutdown();
        }

        Ref<Material> MaterialAsset;
        Ref<AssetSaveTracker> Saves = CreateRef<AssetSaveTracker>();
        Ref<MaterialInspectorTransaction> Transaction = CreateRef<MaterialInspectorTransaction>();

        void Begin()
        {
            if (UndoRedo::Get().BeginComponentScope(Transaction))
                Transaction->Capture("Assets/Surface.cwmat", MaterialAsset, Saves);
        }
    };
} // namespace

TEST_CASE("Material inspector drags retain their initial state and save undo and redo", "[Editor][MaterialInspector][Undo]")
{
    MaterialUndoScope scope;
    scope.Begin();
    UndoRedo::Get().OnItemInteract({ 41u, true, true, false, true });
    scope.MaterialAsset->SetDecalResponseMask(7u);
    UndoRedo::Get().EndComponentScope();

    scope.Begin();
    UndoRedo::Get().OnItemInteract({ 41u, true, false, false, true });
    scope.MaterialAsset->SetDecalResponseMask(3u);
    UndoRedo::Get().EndComponentScope();

    scope.Begin();
    UndoRedo::Get().OnItemInteract({ 41u, false, false, true, false });
    UndoRedo::Get().EndComponentScope();
    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(scope.MaterialAsset->GetDecalResponseMask() == 255u);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    auto save = scope.Saves->TakeReady();
    REQUIRE(save);
    CHECK(save->Filepath == Path("Assets/Surface.cwmat"));
    CHECK(save->Value == StaticRefCast<Asset>(scope.MaterialAsset));
    scope.Saves->Resolve(save->Filepath, true);

    UndoRedo::Get().Redo();
    CHECK(scope.MaterialAsset->GetDecalResponseMask() == 3u);
    CHECK(scope.Saves->TakeReady().has_value());
}

TEST_CASE("Material group reset is one reversible action", "[Editor][MaterialInspector][Undo]")
{
    MaterialUndoScope scope;
    scope.MaterialAsset->SetDecalResponseMask(3u);
    scope.MaterialAsset->SetAlphaMode(AlphaMode::Mask);
    scope.Begin();
    UndoRedo::Get().OnItemInteract({ 57u, false, false, false, true });
    scope.MaterialAsset->SetDecalResponseMask(255u);
    scope.MaterialAsset->ClearAlphaModeOverride();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(scope.MaterialAsset->GetDecalResponseMask() == 3u);
    CHECK(scope.MaterialAsset->HasAlphaModeOverride());
    CHECK(scope.MaterialAsset->GetAlphaMode() == AlphaMode::Mask);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(scope.MaterialAsset->GetDecalResponseMask() == 255u);
    CHECK_FALSE(scope.MaterialAsset->HasAlphaModeOverride());
}

TEST_CASE("Material inspector ignores no-op edits and finishes edits when selection changes", "[Editor][MaterialInspector][Undo]")
{
    MaterialUndoScope scope;
    scope.Begin();
    UndoRedo::Get().OnItemInteract({ 57u, false, false, false, true });
    scope.MaterialAsset->SetDecalResponseMask(255u);
    UndoRedo::Get().EndComponentScope();
    CHECK_FALSE(UndoRedo::Get().CanUndo());

    scope.Begin();
    UndoRedo::Get().OnItemInteract({ 41u, true, true, false, true });
    scope.MaterialAsset->SetDecalResponseMask(7u);
    UndoRedo::Get().EndComponentScope();
    UndoRedo::Get().FinishComponentScope(scope.Transaction);
    scope.Transaction->Reset();
    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(scope.MaterialAsset->GetDecalResponseMask() == 255u);
}
