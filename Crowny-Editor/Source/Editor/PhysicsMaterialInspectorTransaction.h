#pragma once

#include "Editor/AssetSaveTracker.h"
#include "Editor/UndoRedo.h"

#include "Crowny/Physics/PhysicsMaterial.h"

namespace Crowny
{
    class PhysicsMaterialInspectorTransaction final : public RetainedUndoActionFactory
    {
    public:
        void Capture(const Path& path, const Ref<Asset>& asset, const Ref<AssetSaveTracker>& saveTracker);

        Ref<UndoAction> Build() const override;
        void Reset() override;

    private:
        Path m_Path;
        Ref<Asset> m_Asset;
        Ref<AssetSaveTracker> m_SaveTracker;
        PhysicsMaterialData m_Before;
        bool m_HasBefore = false;
    };
} // namespace Crowny
