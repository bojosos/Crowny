#pragma once

#include "Crowny/Common/DataStream.h"

namespace Crowny
{

    struct AudioDataInfo;

    class AudioDecoder
    {
    public:
        virtual ~AudioDecoder() = default;
        virtual bool Open(const Ref<DataStream>& stream, AudioDataInfo& info, uint32_t offset = 0) = 0;
        virtual void Seek(uint32_t offset) = 0;
        virtual uint32_t Read(uint8_t* samples, uint32_t numSamples) = 0;
        virtual bool IsValid(const Ref<DataStream>& stream, uint32_t offset = 0) = 0;
    };

} // namespace Crowny