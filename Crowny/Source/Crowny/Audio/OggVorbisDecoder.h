#pragma once

#include "Crowny/Common/DataStream.h"

#include <vorbis/vorbisfile.h>

namespace Crowny
{

    struct AudioDataInfo;

    struct OggDecoderData
    {
        Ref<DataStream> Stream;
        uint32_t Offset;
    };

    class OggVorbisDecoder
    {
    public:
        OggVorbisDecoder();
        ~OggVorbisDecoder();
        bool Open(const Ref<DataStream>& stream, AudioDataInfo& info, uint32_t offset = 0);
        uint32_t Read(uint8_t* samples, uint32_t count);
        void Seek(uint32_t offset);
        bool IsValid(const Ref<DataStream>& stream, uint32_t offset);

    private:
        OggDecoderData m_DecoderData;
        OggVorbis_File m_OggVorbisFile;
        uint32_t m_ChannelCount;
    };

} // namespace Crowny