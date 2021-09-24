#include "cwpch.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/WaveDecoder.h"

namespace Crowny
{

#define WAVE_FORMAT_PCM 0x0001
#define WAVE_FORMAT_EXT 0xFFFE

    bool WaveDecoder::IsValid(const Ref<DataStream>& stream, uint32_t offset)
    {
        stream->Seek(offset);
        int8_t header[MAIN_HEADER_SIZE];
        if (stream->Read(header, sizeof(header)) < sizeof(header))
            return false;
        return (header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' && header[8] == 'W' &&
                header[9] == 'A' && header[10] == 'V' && header[11] == 'E');
    }

    bool WaveDecoder::Open(const Ref<DataStream>& stream, AudioDataInfo& info, uint32_t offset)
    {
        if (stream == nullptr)
            return false;
        m_Stream = stream;
        m_Stream->Seek(offset + MAIN_HEADER_SIZE);
        if (!ParseHeader(info))
        {
            CW_ENGINE_ERROR("File is not a WAVE file");
            return false;
        }
        return true;
    }

    void WaveDecoder::Seek(uint32_t offset) { m_Stream->Seek(m_DataOffset + offset * m_BytesPerSample); }

    uint32_t WaveDecoder::Read(uint8_t* samples, uint32_t numSamples)
    {
        uint32_t numRead = (uint32_t)m_Stream->Read(samples, numSamples * m_BytesPerSample);
        if (m_BytesPerSample == 1)
        {
            for (uint32_t i = 0; i < numRead; i++)
            {
                int8_t value = samples[i] - 128;
                samples[i] = *((uint8_t*)&value);
            }
        }
        return numRead;
    }

    bool WaveDecoder::ParseHeader(AudioDataInfo& info)
    {
        bool inData = false;
        while (!inData)
        {
            uint8_t subChunkId[4];
            if (m_Stream->Read(subChunkId, sizeof(subChunkId)) != sizeof(subChunkId))
                return false;
            uint32_t subChunkSize = 0;
            if (m_Stream->Read(&subChunkSize, sizeof(subChunkSize)) != sizeof(subChunkSize))
                return false;

            if (subChunkId[0] == 'f' && subChunkId[1] == 'm' && subChunkId[2] == 't' && subChunkId[3] == ' ')
            {
                uint16_t format = 0;
                if (m_Stream->Read(&format, sizeof(format)) != sizeof(format))
                    return false;
                if (format != WAVE_FORMAT_PCM && format != WAVE_FORMAT_EXT)
                {
                    CW_ENGINE_ERROR("Wave file does not contain raw PCM data. Not supported.");
                    return false;
                }

                uint16_t numChannels = 0;
                if (m_Stream->Read(&numChannels, sizeof(numChannels)) != sizeof(numChannels))
                    return false;

                uint32_t sampleRate = 0;
                if (m_Stream->Read(&sampleRate, sizeof(sampleRate)) != sizeof(sampleRate))
                    return false;

                uint32_t byteRate = 0;
                if (m_Stream->Read(&byteRate, sizeof(byteRate)) != sizeof(byteRate))
                    return false;

                uint16_t blockAlign = 0;
                if (m_Stream->Read(&blockAlign, sizeof(blockAlign)) != sizeof(blockAlign))
                    return false;

                uint16_t bitDepth = 0;
                if (m_Stream->Read(&bitDepth, sizeof(bitDepth)) != sizeof(bitDepth))
                    return false;

                info.NumChannels = numChannels;
                info.SampleRate = sampleRate;
                info.BitDepth = bitDepth;
                if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
                {
                    CW_ENGINE_ERROR("Unsupported bit depth: {0}", bitDepth);
                    return false;
                }

                if (format == WAVE_FORMAT_EXT)
                {
                    uint16_t extSize = 0;
                    if (m_Stream->Read(&extSize, sizeof(extSize)) != sizeof(extSize))
                        return false;
                    if (extSize != 22)
                    {
                        CW_ENGINE_ERROR("Wave file does not contain raw PCM data. Not supported.");
                        return false;
                    }

                    uint16_t validBitDepth = 0;
                    if (m_Stream->Read(&validBitDepth, sizeof(validBitDepth)) != sizeof(validBitDepth))
                        return false;
                    uint8_t subFormat[16];
                    if (m_Stream->Read(subFormat, sizeof(subFormat)) != sizeof(subFormat))
                        return false;

                    std::memcpy(&format, subFormat, sizeof(format));
                    if (format != WAVE_FORMAT_PCM)
                    {
                        CW_ENGINE_INFO("Wave file does not contain raw PCM data. Not supported.");
                        return false;
                    }
                }
                m_BytesPerSample = bitDepth / 8;
            }
            else if (subChunkId[0] == 'd' && subChunkId[1] == 'a' && subChunkId[2] == 't' && subChunkId[3] == 'a')
            {
                info.NumSamples = subChunkSize / m_BytesPerSample;
                m_DataOffset = (uint32_t)m_Stream->Tell();
                inData = true;
            }
            else
            {
                m_Stream->Skip(subChunkSize);
                if (m_Stream->Eof())
                    return false;
            }
        }

        return true;
    }

} // namespace Crowny