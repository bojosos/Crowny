#include "cwpch.h"

#include "Crowny/Audio/AudioUtils.h"

namespace Crowny
{

    ALenum AudioUtils::GetOpenALFormat(uint32_t numChannels, uint32_t bitDepth)
    {
        switch (bitDepth)
        {
        case 8: {
            switch (numChannels)
            {
            case 1:
                return AL_FORMAT_MONO8;
            case 2:
                return AL_FORMAT_STEREO8;
            case 4:
                return alGetEnumValue("AL_FORMAT_QUEAD8");
            case 6:
                return alGetEnumValue("AL_FORMAT_51CHN8");
            case 7:
                return alGetEnumValue("AL_FORMAT_61CHN8");
            case 8:
                return alGetEnumValue("AL_FORMAT_71CHN8");
            default:
                CW_ENGINE_ASSERT(false);
                return 0;
            }
        }
        case 16: {
            switch (numChannels)
            {
            case 1:
                return AL_FORMAT_MONO16;
            case 2:
                return AL_FORMAT_STEREO16;
            case 4:
                return alGetEnumValue("AL_FORMAT_QUEAD16");
            case 6:
                return alGetEnumValue("AL_FORMAT_51CHN16");
            case 7:
                return alGetEnumValue("AL_FORMAT_61CHN16");
            case 8:
                return alGetEnumValue("AL_FORMAT_71CHN16");
            default:
                CW_ENGINE_ASSERT(false);
                return 0;
            }
        }
        case 32: {
            switch (numChannels)
            {
            case 1:
                return alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
            case 2:
                return alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
            case 4:
                return alGetEnumValue("AL_FORMAT_QUAD32");
            case 6:
                return alGetEnumValue("AL_FORMAT_51CHN32");
            case 7:
                return alGetEnumValue("AL_FORMAT_61CHN32");
            case 8:
                return alGetEnumValue("AL_FORMAT_71CHN32");
            default:
                CW_ENGINE_ASSERT(false);
                return 0;
            }
        }
        default:
            CW_ENGINE_ASSERT(false);
            return 0;
        }
    }

    void Convert8To32Bits(const int8_t* input, int32_t* output, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            int8_t val = input[i];
            output[i] = val << 24;
        }
    }

    void Convert16To32Bits(const int16_t* input, int32_t* output, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; i++)
            output[i] = input[i] << 16;
    }

    int32_t AudioUtils::Convert24To32Bits(const uint8_t* in) { return (in[2] << 24) | (in[1] << 16) | (in[0] << 8); }

    void Convert24To32Bits(const uint8_t* input, int32_t* output, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            output[i] = AudioUtils::Convert24To32Bits(input);
            input += 3;
        }
    }

    void Convert32To8Bits(const int32_t* input, uint8_t* output, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; i++)
            output[i] = (int8_t)(input[i] >> 24);
    }

    void Convert32To16Bits(const int32_t* input, uint8_t* output, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; i++)
            output[i] = (int16_t)(input[i] >> 16);
    }

    void Convert32To24Bits(const int32_t input, uint8_t* output)
    {
        uint32_t val = *(uint32_t*)&input;
        output[0] = (val >> 8) & 0x000000FF;
        output[1] = (val >> 16) & 0x000000FF;
        output[2] = (val >> 24) & 0x000000FF;
    }

    void Convert32To24Bits(const int32_t* input, uint8_t* output, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            Convert32To24Bits(input[i], output);
            output += 3;
        }
    }

    void AudioUtils::ConvertBitDepth(const uint8_t* samples, uint32_t inBitDepth, uint8_t* output, uint32_t outBitDepth,
                                     uint32_t numSamples)
    {
        int32_t* src = nullptr;
        const bool needNewBuffer = inBitDepth != 32;
        if (needNewBuffer)
            src = new int32_t[numSamples * sizeof(int32_t)];
        else
            src = (int32_t*)samples;

        // convert to 32-bit, then to the desirerd format
        switch (inBitDepth)
        {
        case 8:
            Convert8To32Bits((int8_t*)samples, src, numSamples);
            break;
        case 16:
            Convert16To32Bits((int16_t*)samples, src, numSamples);
            break;
        case 24:
            ::Crowny::Convert24To32Bits(samples, src, numSamples);
            break;
        case 32:
            break;
        default:
            CW_ENGINE_ASSERT(false);
            break;
        }

        switch (outBitDepth)
        {
        case 8:
            Convert32To8Bits(src, output, numSamples);
            break;
        case 16:
            Convert32To16Bits(src, output, numSamples);
            break;
        case 24:
            Convert32To24Bits(src, output, numSamples);
            break;
        case 32:
            std::memcpy(output, src, numSamples * sizeof(uint32_t));
            break;
        default:
            CW_ENGINE_ASSERT(false);
            break;
        }

        if (needNewBuffer)
        {
            delete[] src;
            src = nullptr;
        }
    }

    void AudioUtils::ConvertToFloat(const uint8_t* samples, uint32_t inBitDepth, float* output, uint32_t numSamples)
    {
        if (inBitDepth == 8)
        {
            for (uint32_t i = 0; i < numSamples; i++)
            {
                int8_t sample = *(int8_t*)samples;
                output[i] = sample / 127.0f;
                samples++;
            }
        }
        else if (inBitDepth == 16)
        {
            for (uint32_t i = 0; i < numSamples; i++)
            {
                int16_t sample = *(int16_t*)samples;
                output[i] = sample / 32767.0f;
                samples += 2;
            }
        }
        else if (inBitDepth == 24)
        {
            for (uint32_t i = 0; i < numSamples; i++)
            {
                int32_t sample = AudioUtils::Convert24To32Bits(samples);
                output[i] = sample / 2147483647.0f;
                samples += 3;
            }
        }
        else if (inBitDepth == 32)
        {
            for (uint32_t i = 0; i < numSamples; i++)
            {
                int32_t sample = *(int32_t*)samples;
                output[i] = sample / 2147483647.0f;
                samples += 4;
            }
        }
        else
            CW_ENGINE_ASSERT(false);
    }

    void ConvertToMono8(const int8_t* input, uint8_t* output, uint32_t bitDepth, uint32_t numSamples,
                        uint32_t numChannels)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            int16_t sum = 0;
            for (uint32_t j = 0; j < numChannels; j++)
            {
                sum += *input;
                input++;
            }
            *output = sum / numChannels;
            output++;
        }
    }

    void ConvertToMono16(const int16_t* input, int16_t* output, uint32_t bitDepth, uint32_t numSamples,
                         uint32_t numChannels)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            int32_t sum = 0;
            for (uint32_t j = 0; j < numChannels; j++)
            {
                sum += *input;
                input++;
            }
            *output = sum / numChannels;
            output++;
        }
    }

    void ConvertToMono24(const uint8_t* input, uint8_t* output, uint32_t bitDepth, uint32_t numSamples,
                         uint32_t numChannels)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            int64_t sum = 0;
            for (uint32_t j = 0; j < numChannels; j++)
            {
                sum += *input;
                input += 3;
            }
            Convert32To24Bits((int32_t)(sum / numChannels), output);
            output += 3;
        }
    }

    void ConvertToMono32(const int32_t* input, int32_t* output, uint32_t bitDepth, uint32_t numSamples,
                         uint32_t numChannels)
    {
        for (uint32_t i = 0; i < numSamples; i++)
        {
            int64_t sum = 0;
            for (uint32_t j = 0; j < numChannels; j++)
            {
                sum += *input;
                input++;
            }
            *output = sum / numChannels;
            output++;
        }
    }

    void AudioUtils::ConvertToMono(const uint8_t* input, uint8_t* output, uint32_t bitDepth, uint32_t numSamples,
                                   uint32_t numChannels)
    {
        switch (bitDepth)
        {
        case 8:
            ConvertToMono8((int8_t*)input, output, bitDepth, numSamples, numChannels);
            break;
        case 16:
            ConvertToMono16((int16_t*)input, (int16_t*)output, bitDepth, numSamples, numChannels);
            break;
        case 24:
            ConvertToMono24(input, output, bitDepth, numSamples, numChannels);
            break;
        case 32:
            ConvertToMono32((int32_t*)input, (int32_t*)output, bitDepth, numSamples, numChannels);
            break;
        default:
            CW_ENGINE_ASSERT(false);
            break;
        }
    }

    bool AudioUtils::CheckOpenALErrors(const String& filename, uint32_t line)
    {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            CW_ENGINE_ERROR("***ERROR*** ({0}: {1})", filename, line);
            switch (error)
            {
            case AL_INVALID_NAME:
                CW_ENGINE_ERROR("AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function");
                break;
            case AL_INVALID_ENUM:
                CW_ENGINE_ERROR("AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function");
                break;
            case AL_INVALID_VALUE:
                CW_ENGINE_ERROR("AL_INVALID_VALUE: an invalid value was passed to an OpenAL function");
                break;
            case AL_INVALID_OPERATION:
                CW_ENGINE_ERROR("AL_INVALID_OPERATION: the requested operation is not valid");
                break;
            case AL_OUT_OF_MEMORY:
                CW_ENGINE_ERROR("AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory");
                break;
            default:
                CW_ENGINE_ERROR("UNKNOWN AL ERROR: {0}", error);
            }
            return false;
        }
        return true;
    }

    bool AudioUtils::CheckOpenALCErrors(const String& filename, uint32_t line, ALCdevice* device)
    {
        ALCenum error = alcGetError(device);
        if (error != ALC_NO_ERROR)
        {
            CW_ENGINE_ERROR("***ERROR*** ({0}: {1})", filename, line);
            switch (error)
            {
            case ALC_INVALID_VALUE:
                CW_ENGINE_ERROR("ALC_INVALID_VALUE: an invalid value was passed to an OpenAL function");
                break;
            case ALC_INVALID_DEVICE:
                CW_ENGINE_ERROR("ALC_INVALID_DEVICE: a bad device was passed to an OpenAL function");
                break;
            case ALC_INVALID_CONTEXT:
                CW_ENGINE_ERROR("ALC_INVALID_CONTEXT: a bad context was passed to an OpenAL function");
                break;
            case ALC_INVALID_ENUM:
                CW_ENGINE_ERROR("ALC_INVALID_ENUM: an unknown enum value was passed to an OpenAL function");
                break;
            case ALC_OUT_OF_MEMORY:
                CW_ENGINE_ERROR("ALC_OUT_OF_MEMORY: an unknown enum value was passed to an OpenAL function");
                break;
            default:
                CW_ENGINE_ERROR("UNKNOWN ALC ERROR: {0}", error);
            }
            return false;
        }
        return true;
    }

} // namespace Crowny