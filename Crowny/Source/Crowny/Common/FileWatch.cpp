#include "cwpch.h"

#include "Crowny/Common/FileWatch.h"
#include "Vendor/filewatch/filewatch.h"

namespace Crowny
{
    FileWatch::FileWatch(const Path& watchPath, FileWatchCallback callback) : m_WatchPath(watchPath), m_FileWatchCallback(callback)
    {
        m_Watch = CreateScope<WatchType>(watchPath, [this](const Path& path, const filewatch::Event changeType) {
            m_FileWatchCallback(m_WatchPath / path, static_cast<Change>(changeType));
        });
    }

    FileWatch::~FileWatch() { m_Watch.reset(); }

} // namespace Crowny
