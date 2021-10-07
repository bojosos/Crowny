#pragma once

#include <AL/al.h>
#include <AL/alc.h>

namespace Crowny
{

    class AudioUtils
    {
    public:
        static ALenum GetOpenALFormat(uint32_t numChannels, uint32_t bitDepth);
        static void ConvertBitDepth(const uint8_t* samples, uint32_t inBitDepth, uint8_t* output, uint32_t outBitDepth,
                                    uint32_t numSamples);
        static void ConvertToFloat(const uint8_t* samples, uint32_t inBitDepth, float* output, uint32_t numSamples);
        static void ConvertToMono(const uint8_t* samples, uint8_t* output, uint32_t bitDepth, uint32_t numSamples,
                                  uint32_t numChannels);
        static bool CheckOpenALErrors(const String& filename, const std::uint_fast32_t line);
        static bool CheckOpenALCErrors(const String& filename, const std::uint_fast32_t line, ALCdevice* device);

        // Needed for OggVorbisEncoder
        static int32_t Convert24To32Bits(const uint8_t* in);
    };

} // namespace Crowny