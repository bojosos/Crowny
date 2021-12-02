#pragma once

#include "Crowny/Audio/OggVorbisDecoder.h"

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/DataStream.h"

namespace Crowny
{

    struct AudioDataInfo
    {
        uint32_t NumSamples;
        uint32_t SampleRate;
        uint32_t NumChannels;
        uint32_t BitDepth;
    };

    enum class AudioFormat : uint8_t
    {
        PCM = 0,
        VORBIS = 1
    };

    enum class AudioReadMode : uint8_t
    {
        LoadDecompressed = 0,
        LoadCompressed = 1,
        Stream = 2
    };

    struct AudioClipDesc
    {
        AudioReadMode ReadMode = AudioReadMode::LoadDecompressed;
        AudioFormat Format = AudioFormat::VORBIS;
        uint32_t Frequency = 44100;
        uint32_t BitDepth = 16;
        uint32_t NumChannels = 2;
        bool Is3D = true;
        bool KeepSourceData = true; // Need it for import, otherwise keeping source data is controlled by Resource::Load
    };

    class AudioClip : public Asset
    {
    public:
        AudioClip() = default;
        AudioClip(const Ref<DataStream>& stream, uint32_t streamSize, uint32_t numSamples, const AudioClipDesc& desc);
        ~AudioClip() = default;

        virtual void Init() override;

        float GetLength() const { return m_Length; }
        uint32_t GetNumSamples() const { return m_NumSamples; }
        const AudioClipDesc& GetDesc() const { return m_Desc; }
        void GetSamples(uint8_t* samples, uint32_t offset, uint32_t count) const;
        Ref<DataStream> GetSourceStream(uint32_t& size) const;
        uint32_t GetOpenALBuffer() const { return m_BufferID; }
        bool Is3D() const { return m_Desc.Is3D; }
        
        float GetFrequency() const { return m_Desc.Frequency; }
        uint32_t GetNumChannels() const { return m_Desc.NumChannels; }

    private:
        CW_SERIALIZABLE(AudioClip);

        AudioClipDesc m_Desc;
        float m_Length = 0.0f;
        mutable OggVorbisDecoder m_VorbisReader;
        bool m_NeedsDecompression = false;
        uint32_t m_BufferID = (uint32_t)-1;
        uint32_t m_NumSamples;
        uint32_t m_StreamSize;
        uint32_t m_StreamOffset = 0;

        Ref<DataStream> m_StreamData;

        mutable Ref<DataStream> m_SourceStreamData;
        uint32_t m_SourceStreamSize = 0;
    };

} // namespace Crowny
