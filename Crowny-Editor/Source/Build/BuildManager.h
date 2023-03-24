#pragma once

#include "Crowny/Common/Module.h"
#include "Build/PlatformInfo.h"

namespace Crowny
{

    struct BuildData
    {
        BuildData();

        PlatformType ActivePlatform;
        Vector<Ref<PlatformInfo>> PlatformData;
    };

	class BuildManager : public Module<BuildManager>
	{
    public:
        BuildManager();

        const Vector<PlatformType>& GetAvailablePlatforms() const;
        void SetActivePlatformInfo(PlatformType type);

        Ref<PlatformInfo> GetActivePlatformInfo() const;
        Ref<PlatformInfo> GetPlatformInfo(PlatformType type) const;
        Vector<String> GetBaseAssemblies(PlatformType type) const;
        const String& GetDefines(PlatformType platform) const;
        PlatformType GetActivePlatform() const;

    private:
        Ref<BuildData> m_BuildData;
	};
}