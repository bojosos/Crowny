#pragma once

#include "Editor/AssetSaveTracker.h"
#include "Editor/UndoRedo.h"

namespace Crowny
{
    // Value snapshots for authored assets exposing GetData()/SetData().
    template <typename AssetT, typename Data> class AssetDataInspectorTransaction final : public RetainedUndoActionFactory
    {
        class Edit final : public UndoAction
        {
        public:
            Edit(Path path, Ref<AssetT> asset, Ref<AssetSaveTracker> saves, Data before, Data after)
              : UndoAction("Edit asset"), m_Path(std::move(path)), m_Asset(std::move(asset)), m_Saves(std::move(saves)), m_Before(std::move(before)),
                m_After(std::move(after))
            {
            }
            void Commit() override { Apply(m_After); }
            void Revert() override { Apply(m_Before); }

        private:
            void Apply(const Data& data)
            {
                if (m_Asset && m_Asset->SetData(data) && m_Saves)
                    m_Saves->Queue(m_Path, m_Asset);
            }
            Path m_Path;
            Ref<AssetT> m_Asset;
            Ref<AssetSaveTracker> m_Saves;
            Data m_Before, m_After;
        };

    public:
        void Capture(const Path& path, const Ref<AssetT>& asset, const Ref<AssetSaveTracker>& saves)
        {
            Reset();
            if (!asset || path.empty() || !saves)
                return;
            m_Path = path;
            m_Asset = asset;
            m_Saves = saves;
            m_Before = asset->GetData();
        }
        Ref<UndoAction> Build() const override
        {
            if (!m_Asset || m_Before == m_Asset->GetData())
                return {};
            return CreateRef<Edit>(m_Path, m_Asset, m_Saves, m_Before, m_Asset->GetData());
        }
        void Reset() override
        {
            m_Path.clear();
            m_Asset = nullptr;
            m_Saves = nullptr;
            m_Before = {};
        }

    private:
        Path m_Path;
        Ref<AssetT> m_Asset;
        Ref<AssetSaveTracker> m_Saves;
        Data m_Before;
    };
} // namespace Crowny
