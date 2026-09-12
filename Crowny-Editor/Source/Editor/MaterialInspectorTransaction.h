#pragma once

#include "Crowny/Renderer/Material.h"
#include "Editor/AssetSaveTracker.h"
#include "Editor/UndoRedo.h"

namespace Crowny
{
    class MaterialInspectorSnapshot;

    class MaterialInspectorTransaction final : public RetainedUndoActionFactory
    {
    public:
        void Capture(const Path& path, const Ref<Material>& material, const Ref<AssetSaveTracker>& saveTracker);
        void BeforeItemInteraction(const UndoItemInteraction& interaction) override;
        Ref<UndoAction> Build() const override;
        void Reset() override;

    private:
        Path m_Path;
        Ref<Material> m_Material;
        Ref<AssetSaveTracker> m_SaveTracker;
        Ref<MaterialInspectorSnapshot> m_Before;
    };
} // namespace Crowny
