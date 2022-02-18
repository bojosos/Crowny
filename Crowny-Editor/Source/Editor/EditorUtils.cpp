#include "cwepch.h"

#include "Editor/EditorUtils.h"

namespace Crowny
{
    Path EditorUtils::GetUniquePath(const Path& path, FileNamingScheme scheme)
    {
        String cleanPath = Path(path).replace_extension("");
        String ext = path.extension();

        int idx = 1;
        size_t sepIdx;
        switch (scheme)
        {
        case FileNamingScheme::BracesIdx:
            sepIdx = cleanPath.rfind("(");
            break;
        case FileNamingScheme::UnderscoreIdx:
            sepIdx = cleanPath.rfind("_");
            break;
        case FileNamingScheme::DotIdx:
            sepIdx = cleanPath.rfind(".");
            break;
        }

        if (sepIdx != String::npos)
        {
            String numStr =
              cleanPath.substr(sepIdx + 1, cleanPath.size() - sepIdx - (scheme == FileNamingScheme::BracesIdx ? 2 : 1));
            uint32_t idx = StringUtils::ParseInt(numStr);
            cleanPath = cleanPath.substr(0, sepIdx);
            idx++;
        }

        String dest = path;
        CW_ENGINE_INFO(dest);
        while (fs::exists(dest))
        {
            CW_ENGINE_INFO(dest);
            switch (scheme)
            {
            case FileNamingScheme::BracesIdx:
                dest = cleanPath + (true ? " (" : "(") + std::to_string(idx) + ")" + ext;
                break;
            case FileNamingScheme::UnderscoreIdx:
                dest = cleanPath + "_" + std::to_string(idx) + ext;
                break;
            case FileNamingScheme::DotIdx:
                dest = cleanPath + "." + std::to_string(idx) + ext;
                break;
            }

            idx++;
        }
        return dest;
    }
} // namespace Crowny