#include "cwpch.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/OggVorbisDecoder.h"

#include <vorbis/codec.h>

namespace Crowny
{

    size_t OggRead(void* ptr, size_t size, size_t nmemb, void* data)
    {
        OggDecoderData* decoderData = static_cast<OggDecoderData*>(data);
        size_t read = static_cast<size_t>(decoderData->Stream->Read(ptr, size * nmemb));
        if (read == 1)
            return 0;
        return read;
    }

    int OggSeek(void* data, ogg_int64_t offset, int whence)
    {
        OggDecoderData* decoderData = static_cast<OggDecoderData*>(data);
        switch (whence)
        {
        case SEEK_SET:
            offset += decoderData->Offset;
            break;
        case SEEK_CUR:
            offset += decoderData->Stream->Tell();
            break;
        case SEEK_END:
            offset = std::max(0, (int32_t)decoderData->Stream->Size() - 1);
            break;
        }
        decoderData->Stream->Seek((uint32_t)offset);
        return (int)(decoderData->Stream->Tell() - decoderData->Offset);
    }

    long OggTell(void* data)
    {
        OggDecoderData* decoderData = static_cast<OggDecoderData*>(data);
        return (long)(decoderData->Stream->Tell() - decoderData->Offset);
    }

    static ov_callbacks callbacks = { &OggRead, &OggSeek, nullptr, &OggTell };

    OggVorbisDecoder::OggVorbisDecoder() { m_OggVorbisFile.datasource = nullptr; }

    OggVorbisDecoder::~OggVorbisDecoder()
    {
        if (m_OggVorbisFile.datasource != nullptr)
            ov_clear(&m_OggVorbisFile);
    }

    bool OggVorbisDecoder::IsValid(const Ref<DataStream>& stream, uint32_t offset)
    {
        stream->Seek(offset);
        m_DecoderData.Stream = stream;
        m_DecoderData.Offset = offset;
        OggVorbis_File file;
        if (ov_test_callbacks(&m_DecoderData, &file, nullptr, 0, callbacks) == 0)
        {
            ov_clear(&file);
            return true;
        }
        return false;
    }

    bool OggVorbisDecoder::Open(const Ref<DataStream>& stream, AudioDataInfo& info, uint32_t offset)
    {
        if (stream == nullptr)
            return false;

        stream->Seek(offset);
        m_DecoderData.Stream = stream;
        m_DecoderData.Offset = offset;
        FILE* f = fopen("test.ogg", "rb");
        // int status = ov_open_callbacks(&m_DecoderData, &m_OggVorbisFile, nullptr, 0, callbacks);
        int status = ov_open(f, &m_OggVorbisFile, nullptr, 0);
        if (status < 0)
        {
            CW_ENGINE_ERROR("Failed toload Ogg Vorbis file: code {0}.", status);
            return false;
        }

        vorbis_info* vorbisInfo = ov_info(&m_OggVorbisFile, -1);
        info.NumChannels = vorbisInfo->channels;
        info.SampleRate = vorbisInfo->rate;
        info.NumSamples = (uint32_t)(ov_pcm_total(&m_OggVorbisFile, -1) * vorbisInfo->channels);
        info.BitDepth = 16;
        m_ChannelCount = info.NumChannels;
        return true;
    }

    void OggVorbisDecoder::Seek(uint32_t offset) { ov_pcm_seek(&m_OggVorbisFile, offset / m_ChannelCount); }

    uint32_t OggVorbisDecoder::Read(uint8_t* samples, uint32_t count)
    {
        uint32_t numSamples = 0;
        while (numSamples < count)
        {
            int32_t toRead = (int32_t)(count - numSamples) * sizeof(int16_t);
            uint32_t bytesRead = ov_read(&m_OggVorbisFile, (char*)samples, toRead, 0, 2, 1, nullptr);
            if (bytesRead > 0)
            {
                uint32_t samplesRead = bytesRead / sizeof(int16_t);
                numSamples += samplesRead;
                samples += samplesRead * sizeof(int16_t);
            }
            else
                break;
        }

        return numSamples;
    }

} // namespace Crowny
