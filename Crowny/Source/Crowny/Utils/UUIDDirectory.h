#pragma once

#include "Crowny/Common/Uuid.h"

namespace Crowny
{

    class UUIDDirectory
    {
    public:
        UUIDDirectory() = default;
        UUIDDirectory(const Path& path);

        Path GetPath(const UUID42& uuid) const;
        void OnFileWrite(const UUID42& uuid);

        void Refresh();
        void RemovePath(const UUID42& uuid);
        void RemovePath(const Path& path);

    private:
        Path m_Path;
        UnorderedMap<UUID42, Path> m_Uuids;
    };

} // namespace Crowny