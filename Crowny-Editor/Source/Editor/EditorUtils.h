#pragma once

#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

    enum class FileNamingScheme
    {
        BracesIdx,
        UnderscoreIdx,
        DotIdx
    };

    class EditorUtils
    {
    public:
        template <typename T> static time_t FileTimeToCTime(T tp)
        {
            using namespace std::chrono;
            auto sctp = time_point_cast<system_clock::duration>(tp - T::clock::now() + system_clock::now());
            return system_clock::to_time_t(sctp);
        }

        static Path GetUniquePath(const Path& path, FileNamingScheme scheme = FileNamingScheme::BracesIdx);
    };

} // namespace Crowny