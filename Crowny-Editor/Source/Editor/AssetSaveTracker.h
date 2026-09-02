#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/StdHeaders.h"

#include <optional>

namespace Crowny
{
    struct AssetSaveRequest
    {
        Path Filepath;
        Ref<Asset> Value;
    };

    class AssetSaveTracker : public RefCounted
    {
    public:
        void Queue(const Path& path, const Ref<Asset>& asset)
        {
            if (path.empty() || asset == nullptr)
                return;

            PendingSave& pending = m_Pending[path];
            pending.Value = asset;
            pending.Ready = true;
        }

        void Observe(const Path& path, const Ref<Asset>& asset, bool changed, bool interactionActive, bool interactionFinished)
        {
            if (path.empty())
                return;

            auto pending = m_Pending.find(path);
            if (changed)
            {
                if (asset == nullptr)
                    return;
                pending = m_Pending.try_emplace(path).first;
                pending->second.Value = asset;
                if (!interactionActive)
                    pending->second.Ready = true;
            }

            if (interactionFinished && pending != m_Pending.end())
                pending->second.Ready = true;
        }

        void Flush()
        {
            for (auto& entry : m_Pending)
                entry.second.Ready = true;
        }

        std::optional<AssetSaveRequest> TakeReady()
        {
            for (auto& [path, pending] : m_Pending)
            {
                if (!pending.Ready)
                    continue;
                pending.Ready = false;
                return AssetSaveRequest{ path, pending.Value };
            }
            return std::nullopt;
        }

        void Resolve(const Path& path, bool success)
        {
            if (success)
                m_Pending.erase(path);
        }

        bool IsPending(const Path& path) const { return m_Pending.find(path) != m_Pending.end(); }
        size_t GetPendingCount() const { return m_Pending.size(); }

    private:
        struct PendingSave
        {
            Ref<Asset> Value;
            bool Ready = false;
        };

        Map<Path, PendingSave> m_Pending;
    };
} // namespace Crowny
