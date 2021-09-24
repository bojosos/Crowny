#pragma once

#include "Crowny/Audio/AudioDecoder.h"

namespace Crowny
{

    // https://docs.fileformat.com/audio/wav/
    class WaveDecoder : public AudioDecoder
    {
    public:
        WaveDecoder() = default;
        virtual bool Open(const Ref<DataStream>& stream, AudioDataInfo& info, uint32_t offset = 0) override;
        virtual uint32_t Read(uint8_t* samples, uint32_t numSamples) override;
        virtual void Seek(uint32_t offset) override;

        bool IsValid(const Ref<DataStream>& stream, uint32_t offset = 0) override;

    private:
        bool ParseHeader(AudioDataInfo& info);

    private:
        Ref<DataStream> m_Stream;
        uint32_t m_DataOffset = 0;
        uint32_t m_BytesPerSample = 0;

        static const uint32_t MAIN_HEADER_SIZE = 12;
    };

} // namespace Crowny