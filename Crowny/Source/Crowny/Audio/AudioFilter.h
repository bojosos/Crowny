#pragma once

#include "Crowny/Common/Common.h"

#include <AL/al.h>

namespace Crowny
{

    // A lightweight wrapper around an OpenAL filter object. Owned by an AudioSource (one per source).
    // We use a single lowpass/highpass filter on the direct path; AL_FILTER_BANDPASS combines both,
    // so we model the same filter as a pair of (gainHF, gainLF) knobs:
    //   * gainHF < 1.0 -> high frequencies attenuated (low-pass effect)
    //   * gainLF < 1.0 -> low frequencies attenuated (high-pass effect)
    // When both are 1.0 the filter is bypassed entirely (and never allocated).
    class AudioFilter
    {
    public:
        AudioFilter() = default;
        ~AudioFilter();

        AudioFilter(const AudioFilter&) = delete;
        AudioFilter& operator=(const AudioFilter&) = delete;

        // Updates the filter and rebinds it to the given source's AL_DIRECT_FILTER. Lazy-allocates
        // the AL filter object on first non-default value, and frees it when both gains return to
        // 1.0 (so neutral-state sources hold no extra resources).
        void Apply(ALuint sourceId, float gainHF, float gainLF);

        // Drops the filter from the source and frees the AL filter object.
        void Detach(ALuint sourceId);

    private:
        ALuint m_FilterId = 0;
        // 0 = none, AL_FILTER_LOWPASS, AL_FILTER_HIGHPASS, AL_FILTER_BANDPASS
        ALint m_FilterType = 0;
    };

} // namespace Crowny
