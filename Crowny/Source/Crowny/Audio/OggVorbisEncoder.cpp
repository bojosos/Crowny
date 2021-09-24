#include "cwpch.h"

#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Audio/OggVorbisEncoder.h"

namespace Crowny
{

    void OggVorbisEncoder::WriteInternal(uint8_t* samples, uint32_t count)
    {
        // CW_ENGINE_INFO(count);
        if ((m_BufferOffset + count) > BUFFER_SIZE)
            Flush();
        if (count > BUFFER_SIZE)
            m_WriteCallback(samples, count);
        else
        {
            std::memcpy(m_Buffer + m_BufferOffset, samples, count);
            m_BufferOffset += count;
        }
    }

#define WRITE_TO_BUFFER(data, length)                                                                                  \
    if ((m_BufferOffset + length) > BUFFER_SIZE)                                                                       \
        Flush();                                                                                                       \
                                                                                                                       \
    if (length > BUFFER_SIZE)                                                                                          \
        m_WriteCallback(data, length);                                                                                 \
    else                                                                                                               \
    {                                                                                                                  \
        memcpy(m_Buffer + m_BufferOffset, data, length);                                                               \
        m_BufferOffset += length;                                                                                      \
    }

    OggVorbisEncoder::~OggVorbisEncoder() { Close(); }

    bool OggVorbisEncoder::Open(std::function<void(uint8_t*, uint32_t)> writeCallback, uint32_t sampleRate,
                                uint32_t bitDepth, uint32_t numChannels, float quality)
    {
        m_NumChannels = numChannels;
        m_BitDepth = bitDepth;
        m_WriteCallback = writeCallback;
        m_Closed = false;

        ogg_stream_init(&m_OggState, std::rand());
        vorbis_info_init(&m_VorbisInfo);

        // Automatic bitrate management with quality 0.4 (~128 kbps for 44 KHz stereo sound)
        int32_t status = vorbis_encode_init_vbr(&m_VorbisInfo, numChannels, sampleRate, 1.0f);
        if (status != 0)
        {
            CW_ENGINE_ERROR("Failed to write Ogg Vorbis file.");
            Close();
            return false;
        }

        vorbis_analysis_init(&m_VorbisState, &m_VorbisInfo);
        vorbis_block_init(&m_VorbisState, &m_VorbisBlock);

        // Generate header
        vorbis_comment comment;
        vorbis_comment_init(&comment);

        ogg_packet headerPacket, commentPacket, codePacket;
        status = vorbis_analysis_headerout(&m_VorbisState, &comment, &headerPacket, &commentPacket, &codePacket);
        vorbis_comment_clear(&comment);

        if (status != 0)
        {
            CW_ENGINE_ERROR("Failed to write Ogg Vorbis file.");
            Close();
            return false;
        }

        // Write header
        ogg_stream_packetin(&m_OggState, &headerPacket);
        ogg_stream_packetin(&m_OggState, &commentPacket);
        ogg_stream_packetin(&m_OggState, &codePacket);

        ogg_page page;
        while (ogg_stream_flush(&m_OggState, &page) > 0)
        {
            WRITE_TO_BUFFER(page.header, page.header_len);
            WRITE_TO_BUFFER(page.body, page.body_len);
        }

        return true;
    }

    void OggVorbisEncoder::Write(uint8_t* samples, uint32_t numSamples)
    {
        static const uint32_t WRITE_LENGTH = 1024;

        uint32_t numFrames = numSamples / m_NumChannels;
        while (numFrames > 0)
        {
            uint32_t numFramesToWrite = std::min(numFrames, WRITE_LENGTH);
            float** buffer = vorbis_analysis_buffer(&m_VorbisState, numFramesToWrite);

            if (m_BitDepth == 8)
            {
                for (uint32_t i = 0; i < numFramesToWrite; i++)
                {
                    for (uint32_t j = 0; j < m_NumChannels; j++)
                    {
                        int8_t sample = *(int8_t*)samples;
                        float encodedSample = sample / 127.0f;
                        buffer[j][i] = encodedSample;

                        samples++;
                    }
                }
            }
            else if (m_BitDepth == 16)
            {
                for (uint32_t i = 0; i < numFramesToWrite; i++)
                {
                    for (uint32_t j = 0; j < m_NumChannels; j++)
                    {
                        int16_t sample = *(int16_t*)samples;
                        float encodedSample = sample / 32767.0f;
                        buffer[j][i] = encodedSample;

                        samples += 2;
                    }
                }
            }
            else if (m_BitDepth == 24)
            {
                for (uint32_t i = 0; i < numFramesToWrite; i++)
                {
                    for (uint32_t j = 0; j < m_NumChannels; j++)
                    {
                        int32_t sample = AudioUtils::Convert24To32Bits(samples);
                        float encodedSample = sample / 2147483647.0f;
                        buffer[j][i] = encodedSample;

                        samples += 3;
                    }
                }
            }
            else if (m_BitDepth == 32)
            {
                for (uint32_t i = 0; i < numFramesToWrite; i++)
                {
                    for (uint32_t j = 0; j < m_NumChannels; j++)
                    {
                        int32_t sample = *(int32_t*)samples;
                        float encodedSample = sample / 2147483647.0f;
                        buffer[j][i] = encodedSample;

                        samples += 4;
                    }
                }
            }
            else
                assert(false);

            // Signal how many frames were written
            vorbis_analysis_wrote(&m_VorbisState, numFramesToWrite);
            WriteBlocks();

            numFrames -= numFramesToWrite;
        }
    }

    void OggVorbisEncoder::WriteBlocks()
    {
        while (vorbis_analysis_blockout(&m_VorbisState, &m_VorbisBlock) == 1)
        {
            // Analyze and determine optimal bitrate
            vorbis_analysis(&m_VorbisBlock, nullptr);
            vorbis_bitrate_addblock(&m_VorbisBlock);

            // Write block into ogg packets
            ogg_packet packet;
            while (vorbis_bitrate_flushpacket(&m_VorbisState, &packet))
            {
                ogg_stream_packetin(&m_OggState, &packet);

                // If new page, write it to the internal buffer
                ogg_page page;
                while (ogg_stream_flush(&m_OggState, &page) > 0)
                {
                    WRITE_TO_BUFFER(page.header, page.header_len);
                    WRITE_TO_BUFFER(page.body, page.body_len);
                }
            }
        }
    }

    void OggVorbisEncoder::Flush()
    {
        if (m_BufferOffset > 0 && m_WriteCallback != nullptr)
            m_WriteCallback(m_Buffer, m_BufferOffset);

        m_BufferOffset = 0;
    }

    void OggVorbisEncoder::Close()
    {
        if (m_Closed)
            return;

        // Mark end of data and flush any remaining data in the buffers
        vorbis_analysis_wrote(&m_VorbisState, 0);
        WriteBlocks();
        Flush();

        ogg_stream_clear(&m_OggState);
        vorbis_block_clear(&m_VorbisBlock);
        vorbis_dsp_clear(&m_VorbisState);
        vorbis_info_clear(&m_VorbisInfo);

        m_Closed = true;
    }

    Ref<MemoryDataStream> OggVorbisEncoder::PCMToOggVorbis(uint8_t* samples, const AudioDataInfo& info, uint32_t& size,
                                                           float quality)
    {
        struct EncodedBlock
        {
            uint8_t* data;
            uint32_t size;
        };

        std::vector<EncodedBlock> blocks;
        uint32_t totalEncodedSize = 0;
        auto writeCallback = [&](uint8_t* buffer, uint32_t size) {
            EncodedBlock newBlock;
            newBlock.data = new uint8_t[size];
            newBlock.size = size;

            memcpy(newBlock.data, buffer, size);
            blocks.push_back(newBlock);
            totalEncodedSize += size;
        };

        OggVorbisEncoder writer;
        writer.Open(writeCallback, info.SampleRate, info.BitDepth, info.NumChannels, 1.0f);

        writer.Write(samples, info.NumSamples);
        writer.Close();

        auto output = CreateRef<MemoryDataStream>(totalEncodedSize);
        uint32_t offset = 0;
        for (auto& block : blocks)
        {
            memcpy(output->Data() + offset, block.data, block.size);
            offset += block.size;
        }

        size = totalEncodedSize;
        CW_ENGINE_INFO("Total size: {0}", size);
        return output;
    }
    /*

        OggVorbisEncoder::~OggVorbisEncoder()
        {
            Close();
        }

        bool OggVorbisEncoder::Open(std::function<void(uint8_t*, uint32_t)> writeCallback, uint32_t sampleRate, uint32_t
       bitDepth, uint32_t numChannels, float quality = 1.0f)
        {
            m_NumChannels = numChannels;
            m_BitDepth = bitDepth;
            m_WriteCallback = writeCallback;
            m_Closed = false;

            ogg_stream_init(&m_OggState, std::rand());
            vorbis_info_init(&m_VorbisInfo);

            int32_t status = vorbis_encode_init_vbr(&m_VorbisInfo, numChannels, sampleRate, quality); // Variable
       bitrate if (status != 0)
            {
                CW_ENGINE_ERROR("Ogg Vorbis encoding failed");
                Close();
                return false;
            }

            vorbis_analysis_init(&m_VorbisState, &m_VorbisInfo);
            vorbis_block_init(&m_VorbisState, &m_VorbisBlock);

            vorbis_comment comment;
            vorbis_comment_init(&comment);

            ogg_packet headerPacket, commentPacket, codePacket;
            status = vorbis_analysis_headerout(&m_VorbisState, &comment, &headerPacket, &commentPacket, &codePacket);
            vorbis_comment_clear(&comment);

            if (status != 0)
            {
                CW_ENGINE_ERROR("Ogg Vorbis encoding failed");
                Close();
                return false;
            }

            ogg_stream_packetin(&m_OggState, &headerPacket);
            ogg_stream_packetin(&m_OggState, &commentPacket);
            ogg_stream_packetin(&m_OggState, &codePacket);

            ogg_page page;
            while (ogg_stream_flush(&m_OggState, &page) > 0)
            {
                WriteInternal(page.header, page.header_len);
                WriteInternal(page.body, page.body_len);
            }

            return true;
        }

        void OggVorbisEncoder::Write(uint8_t* samples, uint32_t numSamples)
        {
            static const uint32_t WRITE_LENGTH = 1024;

            uint32_t numFrames = numSamples / m_NumChannels;
            while (numFrames > 0)
            {
                uint32_t numFramesToWrite = std::min(numFrames, WRITE_LENGTH);
                float** buffer = vorbis_analysis_buffer(&m_VorbisState, numFramesToWrite);

                if (m_BitDepth == 8)
                {
                    for (uint32_t i = 0; i < numFramesToWrite; i++)
                    {
                        for (uint32_t j = 0; j < m_NumChannels; j++)
                        {
                            int8_t sample = *(int8_t*)samples;
                            float encoded = sample / 127.0f;
                            buffer[j][i] = sample;
                            samples++;
                        }
                    }
                }
                else if (m_BitDepth == 16)
                {
                    for (uint32_t i = 0; i < numFramesToWrite; i++)
                    {
                        for (uint32_t j = 0; j < m_NumChannels; j++)
                        {
                            int16_t sample = *(int16_t*)samples;
                            float encoded = sample / 32767.0f;
                            buffer[j][i] = sample;
                            samples += 2;
                        }
                    }
                }
                else if (m_BitDepth == 24)
                {
                    for (uint32_t i = 0; i < numFramesToWrite; i++)
                    {
                        for (uint32_t j = 0; j < m_NumChannels; j++)
                        {
                            int32_t sample = AudioUtils::Convert24To32Bits(samples);
                            float encoded = sample / 2147483647.0f;
                            buffer[j][i] = sample;
                            samples += 3;
                        }
                    }
                }
                else if (m_BitDepth == 32)
                {
                    for (uint32_t i = 0; i < numFramesToWrite; i++)
                    {
                        for (uint32_t j = 0; j < m_NumChannels; j++)
                        {
                            int32_t sample = *(int32_t*)samples;
                            float encoded = sample / 2147483647.0f;
                            buffer[j][i] = sample;
                            samples += 4;
                        }
                    }
                }

                vorbis_analysis_wrote(&m_VorbisState, numFramesToWrite);
                WriteBlocks();
                numFrames -= numFramesToWrite;
            }
        }

        void OggVorbisEncoder::WriteBlocks()
        {
            while (vorbis_analysis_blockout(&m_VorbisState, &m_VorbisBlock) == 1)
            {
                vorbis_analysis(&m_VorbisBlock, nullptr);
                vorbis_bitrate_addblock(&m_VorbisBlock);

                ogg_packet packet;
                while (vorbis_bitrate_flushpacket(&m_VorbisState, &packet))
                {
                    ogg_stream_packetin(&m_OggState, &packet);
                    ogg_page page;
                    while (ogg_stream_flush(&m_OggState, &page) > 0)
                    {
                        WriteInternal(page.header, page.header_len);
                        WriteInternal(page.body, page.body_len);
                    }
                }
            }
        }

        void OggVorbisEncoder::Flush()
        {
            if (m_BufferOffset > 0 && m_WriteCallback != nullptr)
                m_WriteCallback(m_Buffer, m_BufferOffset);
            m_BufferOffset = 0;
        }

        void OggVorbisEncoder::Close()
        {
            if (m_Closed)
                return;
            vorbis_analysis_wrote(&m_VorbisState, 0);
            WriteBlocks();
            Flush();
            ogg_stream_clear(&m_OggState);
            vorbis_block_clear(&m_VorbisBlock);
            vorbis_dsp_clear(&m_VorbisState);
            vorbis_info_clear(&m_VorbisInfo);
            m_Closed = true;
        }

        Ref<MemoryDataStream> OggVorbisEncoder::PCMToOggVorbis(uint8_t* samples, const AudioDataInfo& info, uint32_t&
       size, float quality)
        {
            struct EncodedBlock
            {
                uint8_t* Data;
                uint32_t Size;
            };

            std::vector<EncodedBlock> encodedBlocks;
            uint32_t totalSize = 0;
            auto writeCallback = [&](uint8_t* buffer, uint32_t size)
            {
                EncodedBlock newBlock;
                newBlock.Data = new uint8_t[size];
                newBlock.Size = size;
                std::memcpy(newBlock.Data, buffer, size);
                encodedBlocks.push_back(newBlock);
                totalSize += size;
            };

            OggVorbisEncoder writer;
            writer.Open(writeCallback, info.SampleRate, info.BitDepth, info.NumChannels, quality);
            writer.Write(samples, info.NumSamples);
            writer.Close();
            auto output = CreateRef<MemoryDataStream>(totalSize);
            uint32_t offset = 0;
            for (auto& block : encodedBlocks)
            {
                std::memcpy(output->Data() + offset, block.Data, block.Size);
                offset += block.Size;
            }

            size = totalSize;
            CW_ENGINE_INFO(totalSize);
            return output;
        }
    */
} // namespace Crowny