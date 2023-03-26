#include "cwepch.h"

#include "Build/BuildManager.h"
#include "Build/PlatformInfo.h"

namespace Crowny
{

    BuildData::BuildData() : ActivePlatform(PlatformType::Windows)
    {
        PlatformData.resize((uint32_t)PlatformType::Count);
        PlatformData[(uint32_t)PlatformType::Windows] =
          CreateRef<PlatformInfo>(); // TODO: Create platform specific infos and add them here.
        PlatformData[(uint32_t)PlatformType::Windows]->Defines =
          "CROWNY_EDITOR;CROWNY_EDITOR_WIN;CROWNY_WIN;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
    }

    BuildManager::BuildManager()
    {
        m_BuildData = CreateRef<BuildData>();
    }

    const Vector<PlatformType>& BuildManager::GetAvailablePlatforms() const
    {
        static const Vector<PlatformType> Platforms = {
            PlatformType::Windows,
            PlatformType::Linux,
            PlatformType::Mac,
            PlatformType::MacM1,
        };
        return Platforms;
    }

    const String& BuildManager::GetDefines(PlatformType type) const
    {
        if ((uint32_t)type < (uint32_t)m_BuildData->PlatformData.size())
            return m_BuildData->PlatformData[(uint32_t)type]->Defines;
        return String();
    }

    Ref<PlatformInfo> BuildManager::GetActivePlatformInfo() const
    {
        return m_BuildData->PlatformData[(uint32_t)m_BuildData->ActivePlatform];
    }

    void BuildManager::SetActivePlatformInfo(PlatformType type)
    {
        if ((uint32_t)type < (uint32_t)PlatformType::Count)
            m_BuildData->ActivePlatform = type;
    }

    Ref<PlatformInfo> BuildManager::GetPlatformInfo(PlatformType type) const
    {
        if ((uint32_t)type < (uint32_t)m_BuildData->PlatformData.size())
            return m_BuildData->PlatformData[(uint32_t)type];
        return nullptr;
    }

    Vector<String> BuildManager::GetBaseAssemblies(PlatformType type) const
    {
        switch (type)
        {
        case PlatformType::Windows:
        default:
            return { u8"mscorlib", u8"System", u8"System.Core" };
        }
    }

    PlatformType BuildManager::GetActivePlatform() const { return m_BuildData->ActivePlatform; }
} // namespace Crowny