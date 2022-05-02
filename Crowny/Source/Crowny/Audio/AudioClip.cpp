#include "cwpch.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioUtils.h"

#include <AL/al.h>

namespace Crowny
{

    AudioClip::AudioClip(const Ref<DataStream>& stream, uint32_t streamSize, uint32_t numSamples,
                         const AudioClipDesc& desc)
      : m_StreamSize(streamSize), m_StreamData(stream), m_NumSamples(numSamples), m_Desc(desc)
    {
        if (stream != nullptr)
            m_StreamOffset = (uint32_t)stream->Tell();
        m_KeepData = desc.KeepSourceData;
        Init();
    }

    void AudioClip::Init()
    {
        AudioDataInfo info;
        info.BitDepth = m_Desc.BitDepth;
        info.NumChannels = m_Desc.NumChannels;
        info.NumSamples = m_NumSamples;
        info.SampleRate = m_Desc.Frequency;

        m_Length = m_NumSamples / m_Desc.NumChannels / (float)m_Desc.Frequency;
        if (m_KeepData)
        {
            m_StreamData->Seek(m_StreamOffset);
            auto memStream = CreateRef<MemoryDataStream>(m_StreamSize);
            m_SourceStreamData = memStream;
            m_StreamData->Read(memStream->Data(), m_StreamSize);
            m_SourceStreamSize = m_StreamSize;
        }

        bool loadDecompressed = m_Desc.ReadMode == AudioReadMode::LoadDecompressed ||
                                (m_Desc.ReadMode == AudioReadMode::LoadCompressed && m_Desc.Format == AudioFormat::PCM);
        if (loadDecompressed)
        {
            Ref<DataStream> stream;
            uint32_t offset = 0;
            if (m_SourceStreamData != nullptr)
                stream = m_SourceStreamData;
            else
            {
                stream = m_StreamData;
                offset = m_StreamOffset;
            }

            uint32_t bufferSize = info.NumSamples * info.BitDepth / 8;
            CW_ENGINE_INFO("Buffer size: ", bufferSize);
            uint8_t* sampleBuffer = new uint8_t[bufferSize];
            if (m_Desc.Format == AudioFormat::VORBIS)
            {
                OggVorbisDecoder reader;
                if (reader.Open(stream, info, offset))
                    reader.Read(sampleBuffer, info.NumSamples);
                else
                    CW_ENGINE_ERROR("Audio file decompression failed.");
            }
            else
            {
                stream->Seek(offset);
                stream->Read(sampleBuffer, bufferSize);
            }
            alGenBuffers(1, &m_BufferID);
            gAudio().WriteToOpenALBuffer(m_BufferID, sampleBuffer, info);
            m_StreamData = nullptr;
            m_StreamOffset = 0;
            m_StreamSize = 0;
            delete[] sampleBuffer;
        }
        else if (m_Desc.ReadMode == AudioReadMode::LoadCompressed)
        {
            if (m_StreamData->IsFile())
            {
                if (m_SourceStreamData != nullptr)
                    m_StreamData = m_SourceStreamData;
                else
                {
                    auto memStream = CreateRef<MemoryDataStream>(m_StreamSize);
                    m_StreamData->Seek(m_StreamOffset);
                    m_StreamData->Read(memStream->Data(), m_StreamSize);
                    m_StreamData = memStream;
                }
                m_StreamOffset = 0;
            }
        }

        if (m_Desc.Format == AudioFormat::VORBIS && m_Desc.ReadMode != AudioReadMode::LoadDecompressed)
        {
            m_NeedsDecompression = true;
            if (m_StreamData != nullptr)
            {
                if (!m_VorbisReader.Open(m_StreamData, info, m_StreamOffset))
                    CW_ENGINE_ERROR("Audio file stream failed.");
            }
        }
    }

    void AudioClip::GetSamples(uint8_t* samples, uint32_t offset, uint32_t count) const
    {
        if (m_StreamData != nullptr)
        {
            if (m_NeedsDecompression)
            {
                m_VorbisReader.Seek(offset);
                m_VorbisReader.Read(samples, count);
            }
            else
            {
                uint32_t bytesPerSample = m_Desc.BitDepth / 8;
                uint32_t size = count * bytesPerSample;
                uint32_t streamOffset = m_StreamOffset + offset * bytesPerSample;
                m_StreamData->Seek(streamOffset);
                m_StreamData->Read(samples, size);
            }
            return;
        }

        if (m_SourceStreamData != nullptr)
        {
            CW_ENGINE_ASSERT(!m_NeedsDecompression)
            uint32_t bytesPerSample = m_Desc.BitDepth / 8;
            uint32_t size = count * bytesPerSample;
            uint32_t streamOffset = offset * bytesPerSample;
            m_SourceStreamData->Seek(streamOffset);
            m_SourceStreamData->Read(samples, size);
            return;
        }
    }

    Ref<DataStream> AudioClip::GetSourceStream(uint32_t& size) const
    {
        size = m_SourceStreamSize;
        m_SourceStreamData->Seek(0);
        return m_SourceStreamData;
    }

} // namespace Crowny
