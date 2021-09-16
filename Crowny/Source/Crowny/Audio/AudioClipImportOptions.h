#pragma once

namespace Crowny
{

    class AudioClipImportOptions : public ImportOptions
    {
    public:
        AudioClipImportOptions() = default;
        AudioFormat Format = AudioFormat::VORBIS;
        AudioReadMode ReadMode = AudioReadMode::LoadDecompressed;
        bool Is3D = true;
        uint32_t BitDepth = 16;
        static Ref<AudioClipImportOptions> Create();
    };

} // namespace Crowny
