#include "cwpch.h"

#include "Crowny/Import/AudioClipImporter.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Audio/OggVorbisDecoder.h"
#include "Crowny/Audio/OggVorbisEncoder.h"
#include "Crowny/Audio/WaveDecoder.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

    bool AudioClipImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "ogg" || lower == "wav";
    }

    bool AudioClipImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> AudioClipImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        Ref<const AudioClipImportOptions> audioImportOptions =
          std::static_pointer_cast<const AudioClipImportOptions>(importOptions);
        AudioDataInfo info;
        uint32_t bytesPerSample;
        uint32_t bufferSize;
        Ref<MemoryDataStream> sampleStream;
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        String ext = filepath.extension().string();
        ext = ext.substr(1, ext.size() - 1); // remove .
        Ref<AudioDecoder> reader;
        if (ext == "ogg")
            reader = CreateRef<OggVorbisDecoder>();
        else if (ext == "wav")
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
        CW_ENGINE_INFO(filepath);
        CW_ENGINE_INFO(filepath.filename().string());
        clip->SetName(filepath.filename().string());
        return clip;
    }

    Ref<ImportOptions> AudioClipImporter::CreateImportOptions() const { return CreateRef<AudioClipImportOptions>(); }
} // namespace Crowny
