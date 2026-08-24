#include "cwepch.h"

#include "Build/BuildManager.h"
#include "Build/PlatformInfo.h"

namespace Crowny
{

    BuildData::BuildData() : ActivePlatform(PlatformType::Windows)
    {
        PlatformData.resize((uint32_t)PlatformType::Count);
        for (uint32_t i = 0; i < (uint32_t)PlatformType::Count; i++)
        {
            PlatformData[i] = CreateRef<PlatformInfo>();
            PlatformData[i]->Type = (PlatformType)i;
        }

        PlatformData[(uint32_t)PlatformType::Windows]->Defines = "CROWNY_WIN;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::Linux]->Defines = "CROWNY_LINUX;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::Mac]->Defines = "CROWNY_MACOS;CROWNY_64;CROWNY_0_0_1;CROWNY_MONO";
        PlatformData[(uint32_t)PlatformType::MacM1]->Defines = "CROWNY_MACOS;CROWNY_ARM64;CROWNY_0_0_1;CROWNY_MONO";
    }

    BuildManager::BuildManager() { m_BuildData = CreateRef<BuildData>(); }

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
        if ((uint32_t)type < (uint32_t)m_BuildData->PlatformData.size() && m_BuildData->PlatformData[(uint32_t)type])
            return m_BuildData->PlatformData[(uint32_t)type]->Defines;
        return m_BuildData->PlatformData[0]->Defines;
    }

    Ref<PlatformInfo> BuildManager::GetActivePlatformInfo() const { return m_BuildData->PlatformData[(uint32_t)m_BuildData->ActivePlatform]; }

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
        // TODO: These should ideally be paths or the project generation should change
        // since this will link the Microsoft ones.
        switch (type)
        {
        case PlatformType::Windows:
        default:
            return { "mscorlib", "System", "System.Core", "System.Collections", "System.IO", "System.Compression", "System.IO.Filesystem" };
        }
    }

    PlatformType BuildManager::GetActivePlatform() const { return m_BuildData->ActivePlatform; }

    const char* BuildManager::GetPlatformName(PlatformType type) const
    {
        switch (type)
        {
        case PlatformType::Windows:
            return "Windows";
        case PlatformType::Linux:
            return "Linux";
        case PlatformType::Mac:
            return "macOS Intel";
        case PlatformType::MacM1:
            return "macOS Apple silicon";
        default:
            return "Unknown";
        }
    }

    BuildValidation BuildManager::ValidateActiveBuild(uint32_t includedAssetCount) const
    {
        BuildValidation validation;
        const Ref<PlatformInfo> info = GetActivePlatformInfo();
        if (!info)
        {
            validation.Errors.push_back("The selected platform has no build configuration.");
            return validation;
        }

        if (info->OutputDirectory.empty())
            validation.Errors.push_back("Choose an output folder.");
        else if (fs::exists(info->OutputDirectory) && !fs::is_directory(info->OutputDirectory))
            validation.Errors.push_back("The output path points to a file.");
        if (info->MainScene.Empty())
            validation.Errors.push_back("Choose a main scene.");
        if (includedAssetCount == 0)
            validation.Warnings.push_back("No assets are marked for inclusion in the build.");
        if (!info->ExportSupported)
            validation.Warnings.push_back("Runtime packaging is not implemented for this platform. Crowny can build game scripts only.");
        return validation;
    }
} // namespace Crowny
