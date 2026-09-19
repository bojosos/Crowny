#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"

namespace Crowny
{
    class Sprite;
    enum class SpriteAnimationMode : uint8_t
    {
        Loop,
        Once,
        PingPong
    };

    struct SpriteAnimationFrame
    {
        UUID SpriteId;
        float Duration = 0.1f;
        bool operator==(const SpriteAnimationFrame&) const = default;
    };

    struct SpriteAnimationClipData
    {
        Vector<SpriteAnimationFrame> Frames;
        SpriteAnimationMode Mode = SpriteAnimationMode::Loop;
        bool operator==(const SpriteAnimationClipData&) const = default;
    };

    class SpriteAnimationClip : public Asset
    {
    public:
        AssetType GetAssetType() const override { return AssetType::SpriteAnimationClip; }
        static AssetType GetStaticType() { return AssetType::SpriteAnimationClip; }
        const SpriteAnimationClipData& GetData() const { return m_Data; }
        // Empty clips are valid authoring assets. Invalid edits leave the clip unchanged.
        bool SetData(const SpriteAnimationClipData& data);
        double GetDuration() const { return m_Ends.empty() ? 0.0 : m_Ends.back(); }
        uint64_t GetRevision() const { return m_Revision; }
        // Returns UINT32_MAX for an empty clip. Time is in seconds.
        uint32_t Sample(double time) const;
        // Retains each resolved frame so looping does not reload released textures.
        // Call on the asset/simulation thread, never during render-thread extraction.
        const AssetHandle<Sprite>& ResolveFrame(uint32_t index) const;

    private:
        CW_SERIALIZABLE(SpriteAnimationClip);
        SpriteAnimationClipData m_Data;
        Vector<double> m_Ends;
        uint64_t m_Revision = 0;
        mutable Vector<AssetHandle<Sprite>> m_Frames;
        mutable Vector<bool> m_FrameLoadAttempted;
    };

    // Value-owned playback state; advancing never loads assets or depends on a view.
    class SpriteAnimationPlayback
    {
    public:
        void Play() { m_Playing = true; }
        void Pause() { m_Playing = false; }
        void Stop();
        void Seek(double time, const SpriteAnimationClip& clip);
        void Advance(double delta, double speed, const SpriteAnimationClip& clip);
        double GetTime() const { return m_Time; }
        bool IsPlaying() const { return m_Playing; }
        uint64_t ConsumeCompletions();

    private:
        double m_Time = 0;
        uint64_t m_Completions = 0;
        bool m_Playing = false;
    };
} // namespace Crowny
