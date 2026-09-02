#include "cwepch.h"

#include "Editor/PhysicsMaterialInspectorTransaction.h"

namespace Crowny
{
    namespace
    {
        bool ReadPhysicsMaterial(const Ref<Asset>& asset, PhysicsMaterialData& data)
        {
            if (asset == nullptr)
                return false;

            if (asset->GetAssetType() == AssetType::PhysicsMaterial2D)
            {
                const Ref<PhysicsMaterial2D> material = StaticRefCast<PhysicsMaterial2D>(asset);
                data = material->GetData();
                return true;
            }
            if (asset->GetAssetType() == AssetType::PhysicsMaterial)
            {
                const Ref<PhysicsMaterial3D> material = StaticRefCast<PhysicsMaterial3D>(asset);
                data = material->GetData();
                return true;
            }
            return false;
        }

        bool PhysicsMaterialEqual(const PhysicsMaterialData& first, const PhysicsMaterialData& second)
        {
            return first.Density == second.Density && first.Friction == second.Friction && first.Restitution == second.Restitution &&
                   first.RestitutionThreshold == second.RestitutionThreshold && first.FrictionCombine == second.FrictionCombine &&
                   first.RestitutionCombine == second.RestitutionCombine;
        }

        class PhysicsMaterialEditAction final : public UndoAction
        {
        public:
            PhysicsMaterialEditAction(Path path, Ref<Asset> asset, Ref<AssetSaveTracker> saveTracker, const PhysicsMaterialData& before,
                                      const PhysicsMaterialData& after)
              : UndoAction("Edit physics material"), m_Path(std::move(path)), m_Asset(std::move(asset)), m_SaveTracker(std::move(saveTracker)),
                m_Before(before), m_After(after)
            {
            }

            void Commit() override { Apply(m_After); }
            void Revert() override { Apply(m_Before); }

        private:
            void Apply(const PhysicsMaterialData& data)
            {
                if (m_Asset == nullptr)
                    return;

                if (m_Asset->GetAssetType() == AssetType::PhysicsMaterial2D)
                    StaticRefCast<PhysicsMaterial2D>(m_Asset)->SetData(data);
                else if (m_Asset->GetAssetType() == AssetType::PhysicsMaterial)
                    StaticRefCast<PhysicsMaterial3D>(m_Asset)->SetData(data);
                else
                    return;

                if (m_SaveTracker != nullptr)
                    m_SaveTracker->Queue(m_Path, m_Asset);
            }

            Path m_Path;
            Ref<Asset> m_Asset;
            Ref<AssetSaveTracker> m_SaveTracker;
            PhysicsMaterialData m_Before;
            PhysicsMaterialData m_After;
        };
    } // namespace

    void PhysicsMaterialInspectorTransaction::Capture(const Path& path, const Ref<Asset>& asset, const Ref<AssetSaveTracker>& saveTracker)
    {
        Reset();
        PhysicsMaterialData before;
        if (path.empty() || saveTracker == nullptr || !ReadPhysicsMaterial(asset, before))
            return;

        m_Path = path;
        m_Asset = asset;
        m_SaveTracker = saveTracker;
        m_Before = before;
        m_HasBefore = true;
    }

    Ref<UndoAction> PhysicsMaterialInspectorTransaction::Build() const
    {
        PhysicsMaterialData after;
        if (!m_HasBefore || !ReadPhysicsMaterial(m_Asset, after) || PhysicsMaterialEqual(m_Before, after))
            return {};

        return CreateRef<PhysicsMaterialEditAction>(m_Path, m_Asset, m_SaveTracker, m_Before, after);
    }

    void PhysicsMaterialInspectorTransaction::Reset()
    {
        m_Path.clear();
        m_Asset = nullptr;
        m_SaveTracker = nullptr;
        m_Before = {};
        m_HasBefore = false;
    }
} // namespace Crowny
