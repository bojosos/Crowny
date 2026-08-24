#include "cwpch.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioUtils.h"

#include <AL/al.h>

namespace Crowny
{

    AudioClip::AudioClip(const Ref<DataStream>& stream, uint32_t streamSize, uint32_t numSamples, const AudioClipDesc& desc)
      : m_StreamSize(streamSize), m_StreamData(stream), m_NumSamples(numSamples), m_Desc(desc)
    {
        if (stream != nullptr)
            m_StreamOffset = (uint32_t)stream->Tell();
        m_KeepData = desc.KeepSourceData;
        Init();
    }

    Ref<AudioClip> AudioClip::Create(const Ref<DataStream>& stream, uint32_t streamSize, uint32_t numSamples, const AudioClipDesc& desc)
    {
        return CreateRef<AudioClip>(stream, streamSize, numSamples, desc);
    }

    AudioClip::~AudioClip()
    {
        if (m_BufferID != static_cast<uint32_t>(-1) && AudioManager::TryGet() && AudioManager::TryGet()->IsAvailable())
            alDeleteBuffers(1, &m_BufferID);
    }

    void AudioClip::Init()
    {
        if (m_BufferID != static_cast<uint32_t>(-1) && AudioManager::TryGet() && AudioManager::TryGet()->IsAvailable())
        {
            alDeleteBuffers(1, &m_BufferID);
            m_BufferID = static_cast<uint32_t>(-1);
        }

        if (m_Desc.NumChannels == 0 || m_Desc.Frequency == 0)
        {
            CW_ENGINE_ERROR("Cannot initialize an audio clip with zero channels or sample rate.");
            m_Length = 0.0f;
            return;
        }

        AudioDataInfo info;
        info.BitDepth = m_Desc.BitDepth;
        info.NumChannels = m_Desc.NumChannels;
        info.NumSamples = m_NumSamples;
        info.SampleRate = m_Desc.Frequency;

        m_Length = static_cast<float>(m_NumSamples) / static_cast<float>(m_Desc.NumChannels) / static_cast<float>(m_Desc.Frequency);
        if (m_StreamData == nullptr)
        {
            if (m_NumSamples != 0)
                CW_ENGINE_ERROR("Cannot initialize an audio clip without sample data.");
            return;
        }

        if (m_KeepData && m_SourceStreamData == nullptr)
        {
            m_StreamData->Seek(m_StreamOffset);
            auto memStream = CreateRef<MemoryDataStream>(m_StreamSize);
            m_SourceStreamData = memStream;
            m_StreamData->Read(memStream->Data(), m_StreamSize);
            m_SourceStreamSize = m_StreamSize;
        }

        const bool loadDecompressed = m_Desc.ReadMode == AudioReadMode::LoadDecompressed ||
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

            const uint32_t bufferSize = AudioUtils::GetBufferSize(info.NumSamples, info.BitDepth);
            Vector<uint8_t> sampleBuffer(bufferSize, 0);
            if (m_Desc.Format == AudioFormat::VORBIS)
            {
                OggVorbisDecoder reader;
                if (reader.Open(stream, info, offset))
                    reader.Read(sampleBuffer.data(), info.NumSamples);
                else
                    CW_ENGINE_ERROR("Audio file decompression failed.");
            }
            else
            {
                stream->Seek(offset);
                stream->Read(sampleBuffer.data(), bufferSize);
            }
            if (AudioManager::TryGet() && AudioManager::TryGet()->IsAvailable())
            {
                alGenBuffers(1, &m_BufferID);
                if (!AudioManager::TryGet()->WriteToOpenALBuffer(m_BufferID, sampleBuffer.data(), info))
                {
                    alDeleteBuffers(1, &m_BufferID);
                    m_BufferID = static_cast<uint32_t>(-1);
                }
            }

            if (!m_KeepData)
            {
                m_StreamData = nullptr;
                m_StreamOffset = 0;
                m_StreamSize = 0;
            }
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

    uint32_t AudioClip::GetBuffer(uint8_t* samples, uint32_t offset, uint32_t count) const
    {
        if (samples == nullptr || count == 0 || offset >= m_NumSamples || m_SourceStreamData == nullptr)
            return 0;

        count = std::min(count, m_NumSamples - offset);
        if (m_Desc.Format == AudioFormat::VORBIS)
        {
            AudioDataInfo info;
            info.BitDepth = m_Desc.BitDepth;
            info.NumChannels = m_Desc.NumChannels;
            info.NumSamples = m_NumSamples;
            info.SampleRate = m_Desc.Frequency;

            OggVorbisDecoder reader;
            if (reader.Open(m_SourceStreamData, info))
            {
                reader.Seek(offset);
                return reader.Read(samples, count);
            }
            else
                CW_ENGINE_ERROR("Audio file decompression failed.");
        }
        else
        {
            const uint32_t bytesPerSample = m_Desc.BitDepth / 8;
            m_SourceStreamData->Seek(offset * bytesPerSample);
            return static_cast<uint32_t>(m_SourceStreamData->Read(samples, count * bytesPerSample) / bytesPerSample);
        }
        return 0;
    }

    uint32_t AudioClip::GetSamples(uint8_t* samples, uint32_t offset, uint32_t count) const
    {
        if (samples == nullptr || count == 0 || offset >= m_NumSamples)
            return 0;
        count = std::min(count, m_NumSamples - offset);

        if (m_StreamData != nullptr)
        {
            if (m_NeedsDecompression)
            {
                m_VorbisReader.Seek(offset);
                return m_VorbisReader.Read(samples, count);
            }
            else
            {
                const uint32_t bytesPerSample = m_Desc.BitDepth / 8;
                const uint32_t size = count * bytesPerSample;
                const uint32_t streamOffset = m_StreamOffset + offset * bytesPerSample;
                m_StreamData->Seek(streamOffset);
                return static_cast<uint32_t>(m_StreamData->Read(samples, size) / bytesPerSample);
            }
        }

        if (m_SourceStreamData != nullptr)
        {
            CW_ENGINE_ASSERT(!m_NeedsDecompression);
            const uint32_t bytesPerSample = m_Desc.BitDepth / 8;
            const uint32_t size = count * bytesPerSample;
            const uint32_t streamOffset = offset * bytesPerSample;
            m_SourceStreamData->Seek(streamOffset);
            return static_cast<uint32_t>(m_SourceStreamData->Read(samples, size) / bytesPerSample);
        }
        return 0;
    }

    Ref<DataStream> AudioClip::GetSourceStream(uint32_t& size) const
    {
        size = m_SourceStreamSize;
        if (m_SourceStreamData != nullptr)
            m_SourceStreamData->Seek(0);
        return m_SourceStreamData;
    }

} // namespace Crowny
