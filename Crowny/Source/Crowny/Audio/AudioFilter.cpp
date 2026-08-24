#include "cwpch.h"

#include "Crowny/Audio/AudioFilter.h"
#include "Crowny/Audio/AudioManager.h"

#include <AL/efx.h>

#include <algorithm>

namespace Crowny
{

    AudioFilter::~AudioFilter()
    {
        if (m_FilterId != 0 && AudioManager::TryGet() && AudioManager::TryGet()->IsEFXAvailable())
        {
            AudioManager::TryGet()->GetEFX().DeleteFilters(1, &m_FilterId);
            m_FilterId = 0;
        }
    }

    void AudioFilter::Apply(ALuint sourceId, float gainHF, float gainLF)
    {
        gainHF = std::clamp(gainHF, 0.0f, 1.0f);
        gainLF = std::clamp(gainLF, 0.0f, 1.0f);

        const bool needsLP = gainHF < 1.0f;
        const bool needsHP = gainLF < 1.0f;

        if (!needsLP && !needsHP)
        {
            Detach(sourceId);
            return;
        }

        if (AudioManager::TryGet() == nullptr || !AudioManager::TryGet()->IsEFXAvailable())
            return;

        const EFX& efx = AudioManager::TryGet()->GetEFX();

        const ALint targetType = (needsLP && needsHP) ? AL_FILTER_BANDPASS : (needsLP ? AL_FILTER_LOWPASS : AL_FILTER_HIGHPASS);

        if (m_FilterId == 0)
        {
            efx.GenFilters(1, &m_FilterId);
            if (alGetError() != AL_NO_ERROR)
            {
                CW_ENGINE_WARN("Failed to allocate AL filter for source.");
                m_FilterId = 0;
                return;
            }
        }

        if (m_FilterType != targetType)
        {
            efx.Filteri(m_FilterId, AL_FILTER_TYPE, targetType);
            m_FilterType = targetType;
        }

        // Push gain values according to filter type.
        switch (targetType)
        {
        case AL_FILTER_LOWPASS:
            efx.Filterf(m_FilterId, AL_LOWPASS_GAIN, 1.0f);
            efx.Filterf(m_FilterId, AL_LOWPASS_GAINHF, gainHF);
            break;
        case AL_FILTER_HIGHPASS:
            efx.Filterf(m_FilterId, AL_HIGHPASS_GAIN, 1.0f);
            efx.Filterf(m_FilterId, AL_HIGHPASS_GAINLF, gainLF);
            break;
        case AL_FILTER_BANDPASS:
            efx.Filterf(m_FilterId, AL_BANDPASS_GAIN, 1.0f);
            efx.Filterf(m_FilterId, AL_BANDPASS_GAINHF, gainHF);
            efx.Filterf(m_FilterId, AL_BANDPASS_GAINLF, gainLF);
            break;
        default:
            break;
        }

        // Rebind to source so OpenAL picks up the new parameters.
        alSourcei(sourceId, AL_DIRECT_FILTER, static_cast<ALint>(m_FilterId));
    }

    void AudioFilter::Detach(ALuint sourceId)
    {
        alSourcei(sourceId, AL_DIRECT_FILTER, AL_FILTER_NULL);
        if (m_FilterId != 0 && AudioManager::TryGet() && AudioManager::TryGet()->IsEFXAvailable())
        {
            AudioManager::TryGet()->GetEFX().DeleteFilters(1, &m_FilterId);
            m_FilterId = 0;
            m_FilterType = 0;
        }
    }

} // namespace Crowny
