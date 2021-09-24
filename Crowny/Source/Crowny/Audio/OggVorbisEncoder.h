#pragma once

#include "Crowny/Common/DataStream.h"
#include "Crowny/Audio/AudioClip.h"

#include <vorbis/vorbisenc.h>

namespace Crowny
{
    
    class OggVorbisEncoder
    {
    public:
        OggVorbisEncoder() = default;
        ~OggVorbisEncoder();

        bool Open(std::function<void(uint8_t*, uint32_t)> writeCallback, uint32_t sampleRate, uint32_t bitDepth, uint32_t numChannels, float quality);

        void Write(uint8_t* samples, uint32_t numSamples);
        void Flush();
        void Close();
        static Ref<MemoryDataStream> PCMToOggVorbis(uint8_t* samples, const AudioDataInfo& info, uint32_t& size, float quality = 1.0f);
    private:
        void WriteBlocks();
        void WriteInternal(uint8_t* samples, uint32_t count);
        static const uint32_t BUFFER_SIZE = 4096;

        std::function<void(uint8_t*, uint32_t)> m_WriteCallback;
        uint8_t m_Buffer[BUFFER_SIZE];
        uint32_t m_BufferOffset = 0;
        uint32_t m_NumChannels = 0;
        uint32_t m_BitDepth = 0;
        bool m_Closed = true;

        ogg_stream_state m_OggState;
        vorbis_info m_VorbisInfo;
        vorbis_dsp_state m_VorbisState;
        vorbis_block m_VorbisBlock;
    };

}