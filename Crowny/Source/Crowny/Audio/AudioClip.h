#pragma once

#include "Crowny/Audio/OggVorbisDecoder.h"
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

    enum class AudioFormat
    {
        VORBIS
    };

    enum class AudioReadMode
    {
        LoadDecompressed,
        LoadCompressed,
        Stream,
    };

    struct AudioClipDesc
    {
        AudioReadMode ReadMode = AudioReadMode::LoadDecompressed;
        AudioFormat Format = AudioFormat::VORBIS;
        uint32_t Frequency = 44100;
        uint32_t BitDepth = 16;
        uint32_t NumChannels = 2;
        bool Is3D = true;
    };

    class AudioClip
    {
    public:
        AudioClip(const Ref<DataStream>& stream, uint32_t streamSize, uint32_t numSamples, const AudioClipDesc& desc);
        float GetLength() const { return m_Length; }
        uint32_t GetNumSamples() const { return m_NumSamples; }
        const AudioClipDesc& GetDesc() const { return m_Desc; }
        void GetSamples(uint8_t* samples, uint32_t offset, uint32_t count) const;
        Ref<DataStream> GetSourceStream(uint32_t& size);
        uint32_t GetOpenALBuffer() const { return m_BufferID; }
        bool Is3D() const { return m_Desc.Is3D; }

    private:
        AudioClipDesc m_Desc;
        float m_Length = 0.0f;
        mutable OggVorbisDecoder m_VorbisReader;
        bool m_NeedsDecompression = false;
        uint32_t m_BufferID = (uint32_t)-1;
        uint32_t m_NumSamples;
        uint32_t m_StreamSize;
        uint32_t m_StreamOffset = 0;

        Ref<DataStream> m_StreamData;

        Ref<DataStream> m_SourceStreamData;
        uint32_t m_SourceStreamSize = 0;
        bool m_KeepSourceData;
    };

} // namespace Crowny