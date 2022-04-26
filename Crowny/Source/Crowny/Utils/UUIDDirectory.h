#pragma once

#include "Crowny/Common/Uuid.h"

namespace Crowny
{

    class UUIDDirectory
    {
    public:
        UUIDDirectory() = default;
        UUIDDirectory(const Path& path);

        Path GetPath(const UUID& uuid) const;
        void OnFileWrite(const UUID& uuid);

        void Refresh();
        void RemovePath(const UUID& uuid);
        void RemovePath(const Path& path);

    private:
        Path m_Path;
        UnorderedMap<UUID, Path> m_Uuids;
    };

} // namespace Crowny