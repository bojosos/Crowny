#pragma once

namespace filewatch
{
    template <typename T> class FileWatch;
}

namespace Crowny
{
    class FileWatch
    {
    public:
        enum Change
        {
            FileAdded,
            FileRemoved,
            FileModified,
            FileOldRenamed,
            FileNewRenamed
        };
        using WatchType = filewatch::FileWatch<Path>;
        using FileWatchCallback = std::function<void(const Path& path, Change changeType)>;
        FileWatch(const Path& watchPath, FileWatchCallback callback);
        ~FileWatch();
    private:
        const Path m_WatchPath;
        FileWatchCallback m_FileWatchCallback;
        Scope<WatchType> m_Watch;
    };
}