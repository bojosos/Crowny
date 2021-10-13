#include "cwpch.h"

#include "Crowny/Import/Importer.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Audio/OggVorbisDecoder.h"
#include "Crowny/Audio/OggVorbisEncoder.h"
#include "Crowny/Audio/WaveDecoder.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

    bool IsFileTypeSupported(const String& ext)
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "ogg" || lower == "wav"; // for now ogg only
    }

    template <> Ref<AudioClip> Importer::Import(const Path& filepath, const Ref<ImportOptions>& importOptions)
    {
        Ref<AudioClipImportOptions> audioImportOptions =
          std::static_pointer_cast<AudioClipImportOptions>(importOptions);
        AudioDataInfo info;
        uint32_t bytesPerSample;
        uint32_t bufferSize;
        Ref<MemoryDataStream> sampleStream;
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        String ext = filepath.extension();
        Ref<AudioDecoder> reader;
        if (ext == "ogg")
            reader = CreateRef<OggVorbisDecoder>();
        if (ext == "wav")
            reader = CreateRef<WaveDecoder>();

        if (reader == nullptr)
            return nullptr;
        if (!reader->IsValid(stream))
            return nullptr;
        if (!reader->Open(stream, info))
            return nullptr;

        bytesPerSample = info.BitDepth / 8;
        bufferSize = info.NumSamples * bytesPerSample;
        sampleStream = CreateRef<MemoryDataStream>(bufferSize);
        reader->Read(sampleStream->Data(), info.NumSamples);
        if (audioImportOptions->Is3D && info.NumChannels > 1) // Convert to mono
        {
            uint32_t numSamplesPerChannel = info.NumSamples / info.NumChannels;
            uint32_t monoBufferSize = numSamplesPerChannel * bytesPerSample;
            Ref<MemoryDataStream> monoStream = CreateRef<MemoryDataStream>(monoBufferSize);

            AudioUtils::ConvertToMono(sampleStream->Data(), monoStream->Data(), info.BitDepth, numSamplesPerChannel,
                                      info.NumChannels);

            info.NumSamples = numSamplesPerChannel;
            info.NumChannels = 1;
            sampleStream = monoStream;
            bufferSize = monoBufferSize;
        }

        if (audioImportOptions->BitDepth != info.BitDepth)
        {
            uint32_t outBufferSize = info.NumSamples * (audioImportOptions->BitDepth / 8);
            auto outStream = CreateRef<MemoryDataStream>(outBufferSize);
            AudioUtils::ConvertBitDepth(sampleStream->Data(), info.BitDepth, outStream->Data(),
                                        audioImportOptions->BitDepth, info.NumSamples);
            info.BitDepth = audioImportOptions->BitDepth;
            sampleStream = outStream;
            bufferSize = outBufferSize;
        }
        if (audioImportOptions->Format == AudioFormat::VORBIS)
            sampleStream =
              OggVorbisEncoder::PCMToOggVorbis(sampleStream->Data(), info, bufferSize, audioImportOptions->Quality);

        AudioClipDesc clipDesc;
        clipDesc.BitDepth = info.BitDepth;
        clipDesc.Format = audioImportOptions->Format;
        clipDesc.NumChannels = info.NumChannels;
        clipDesc.Frequency = info.SampleRate;
        clipDesc.Is3D = audioImportOptions->Is3D;
        clipDesc.ReadMode = audioImportOptions->ReadMode;
        Ref<AudioClip> clip = CreateRef<AudioClip>(sampleStream, bufferSize, info.NumSamples, clipDesc);
        clip->SetName(filepath.filename());
        return clip;
    }
} // namespace Crowny
