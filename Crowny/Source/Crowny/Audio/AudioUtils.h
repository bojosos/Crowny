#pragma once

#include "Crowny/Common/Types.h"

#include <AL/al.h>
#include <AL/alc.h>

namespace Crowny
{

    struct AudioDataInfo;

    struct AudioPCMCapabilities
    {
        bool SupportsFloat32 = false;
        bool SupportsMultichannel = false;
    };

    struct AudioStreamScratch
    {
        // DecodedSamples is owned by the source. The remaining arrays are reused by
        // PrepareOpenALPCM and keep their capacity across buffer refills.
        Vector<uint8_t> DecodedSamples;
        Vector<uint8_t> ConvertedBytes;
        Vector<float> ConvertedFloats;
        Vector<int32_t> ConversionSamples;
    };

    struct PreparedAudioPCM
    {
        // Data aliases either the input buffer or AudioStreamScratch and remains valid
        // only until either backing store is resized.
        const void* Data = nullptr;
        ALsizei Size = 0;
        uint32_t BitDepth = 0;
        bool UsedIntegerFallback = false;
    };

    enum class AudioPCMPreparationStatus : uint8_t
    {
        Ready,
        InvalidInput,
        UnsupportedChannels,
        UnsupportedBitDepth,
        UnsupportedMultichannel,
        SizeOverflow
    };

    class AudioUtils
    {
    public:
        static ALenum GetOpenALFormat(uint32_t numChannels, uint32_t bitDepth);
        static bool TryGetBufferSize(uint32_t numSamples, uint32_t bitDepth, uint32_t& size);
        static uint32_t GetBufferSize(uint32_t numSamples, uint32_t bitDepth);
        static void ConvertSigned8ToUnsigned(const uint8_t* samples, uint8_t* output, uint32_t numSamples);
        static void ConvertBitDepth(const uint8_t* samples, uint32_t inBitDepth, uint8_t* output, uint32_t outBitDepth, uint32_t numSamples);
        static void ConvertBitDepth(const uint8_t* samples, uint32_t inBitDepth, uint8_t* output, uint32_t outBitDepth, uint32_t numSamples,
                                    Vector<int32_t>& temporary);
        static void ConvertToFloat(const uint8_t* samples, uint32_t inBitDepth, float* output, uint32_t numSamples);
        static void ConvertToMono(const uint8_t* samples, uint8_t* output, uint32_t bitDepth, uint32_t numSamples, uint32_t numChannels);
        static AudioPCMPreparationStatus PrepareOpenALPCM(const uint8_t* samples, const AudioDataInfo& info, const AudioPCMCapabilities& capabilities,
                                                          AudioStreamScratch& scratch, PreparedAudioPCM& prepared);
        static bool ShouldResumeAfterGlobalPause(AudioSourceState state);
        static bool CheckOpenALErrors(const String& filename, uint32_t line);
        static bool CheckOpenALCErrors(const String& filename, uint32_t line, ALCdevice* device);

        // Needed for OggVorbisEncoder
        static int32_t Convert24To32Bits(const uint8_t* in);
    };

} // namespace Crowny
