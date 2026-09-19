#include "cwpch.h"

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/Sprite.h"
#include "Crowny/Renderer/SpriteAnimationClip.h"

namespace Crowny
{
    bool SpriteAnimationClip::SetData(const SpriteAnimationClipData& data)
    {
        if (data.Frames.size() > 65536 || data.Mode > SpriteAnimationMode::PingPong)
            return false;
        if (data == m_Data)
            return true;
        Vector<double> ends;
        double duration = 0;
        for (const auto& frame : data.Frames)
        {
            if (!std::isfinite(frame.Duration) || frame.Duration <= 0 || duration + frame.Duration <= duration)
                return false;
            duration += frame.Duration;
            ends.push_back(duration);
        }
        // Endpoints appear once per cycle: 0, 1, 2, 1 rather than 0, 1, 2, 2, 1, 0.
        if (data.Mode == SpriteAnimationMode::PingPong && data.Frames.size() > 2)
            for (size_t index = data.Frames.size() - 2; index > 0; --index)
            {
                if (duration + data.Frames[index].Duration <= duration)
                    return false;
                duration += data.Frames[index].Duration;
                ends.push_back(duration);
            }
        if (duration > std::numeric_limits<float>::max())
            return false;
        m_Data = data;
        m_Ends = std::move(ends);
        m_Frames.clear();
        m_Frames.resize(data.Frames.size());
        m_FrameLoadAttempted.assign(data.Frames.size(), false);
        ++m_Revision;
        return true;
    }

    const AssetHandle<Sprite>& SpriteAnimationClip::ResolveFrame(uint32_t index) const
    {
        static const AssetHandle<Sprite> missing;
        if (index >= m_Frames.size())
            return missing;
        const auto& id = m_Data.Frames[index].SpriteId;
        auto* manager = AssetManager::TryGet();
        if (manager && !id.Empty() && !m_FrameLoadAttempted[index])
        {
            auto frame = manager->GetAssetHandle(id);
            if (!frame)
            {
                Path path;
                AssetFileHeader header;
                // Avoid recursively loading malformed clip -> clip references.
                if (manager->GetAssetPath(id, path) && PeekAssetHeader(path, header) && header.Type == AssetType::Sprite)
                {
                    const auto loaded = manager->LoadFromUUID(id, false);
                    if (loaded)
                        frame = loaded;
                }
            }
            m_Frames[index] = static_asset_cast<Sprite>(frame);
            m_FrameLoadAttempted[index] = true;
        }
        const auto& handle = m_Frames[index].GetHandleData();
        return handle && handle->m_Ptr && handle->m_Ptr->GetAssetType() != AssetType::Sprite ? missing : m_Frames[index];
    }

    uint32_t SpriteAnimationClip::Sample(double time) const
    {
        if (m_Ends.empty())
            return UINT32_MAX;
        if (!std::isfinite(time))
            time = 0;
        if (m_Data.Mode == SpriteAnimationMode::Once)
            time = std::clamp(time, 0.0, GetDuration());
        else
        {
            time = std::fmod(time, GetDuration());
            if (time < 0)
                time += GetDuration();
        }
        const size_t step = std::min<size_t>(std::upper_bound(m_Ends.begin(), m_Ends.end(), time) - m_Ends.begin(), m_Ends.size() - 1);
        return static_cast<uint32_t>(step < m_Data.Frames.size() ? step : 2 * m_Data.Frames.size() - 2 - step);
    }

    void SpriteAnimationPlayback::Stop()
    {
        m_Playing = false;
        m_Time = 0;
        m_Completions = 0;
    }

    void SpriteAnimationPlayback::Seek(double time, const SpriteAnimationClip& clip)
    {
        if (!std::isfinite(time))
            return;
        const double duration = clip.GetDuration();
        if (duration <= 0)
            m_Time = 0;
        else if (clip.GetData().Mode == SpriteAnimationMode::Once)
            m_Time = std::clamp(time, 0.0, duration);
        else
        {
            m_Time = std::fmod(time, duration);
            if (m_Time < 0)
                m_Time += duration;
        }
    }

    void SpriteAnimationPlayback::Advance(double delta, double speed, const SpriteAnimationClip& clip)
    {
        const double duration = clip.GetDuration();
        const double step = delta * speed;
        if (!m_Playing || duration <= 0 || delta < 0 || !std::isfinite(step) || step == 0)
            return;
        Seek(m_Time, clip); // Reimport may change the duration.
        const double next = m_Time + step;
        if (!std::isfinite(next))
            return;
        double completions = 0;
        if (clip.GetData().Mode == SpriteAnimationMode::Once)
        {
            if ((step > 0 && next >= duration) || (step < 0 && next <= 0))
            {
                completions = 1;
                m_Playing = false;
            }
        }
        else
            completions = step > 0 ? std::floor(next / duration) : std::ceil(m_Time / duration) - std::ceil(next / duration);
        const uint64_t remaining = UINT64_MAX - m_Completions;
        m_Completions += completions >= static_cast<double>(remaining) ? remaining : static_cast<uint64_t>(completions);
        Seek(next, clip);
    }

    uint64_t SpriteAnimationPlayback::ConsumeCompletions()
    {
        const auto count = m_Completions;
        m_Completions = 0;
        return count;
    }
} // namespace Crowny
