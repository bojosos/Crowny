#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"

namespace Crowny
{
    bool FileSystem::OpenFileDialog(const FileDialogOptions& options, Vector<Path>& outPaths)
    {
        return OpenFileDialog(options.Type, outPaths, options.Title, options.InitialDirectory, options.Filters, options.DefaultName);
    }

    Vector<DialogFilter> FileSystem::ParseDialogFilters(StringView extensions)
    {
        Vector<DialogFilter> result;
        size_t start = 0;
        while (start <= extensions.size())
        {
            const size_t separator = extensions.find(',', start);
            const size_t end = separator == StringView::npos ? extensions.size() : separator;
            String extension(extensions.substr(start, end - start));
            const size_t first = extension.find_first_not_of(" \t\r\n.");
            const size_t last = extension.find_last_not_of(" \t\r\n");
            if (first != String::npos && last >= first)
            {
                extension = extension.substr(first, last - first + 1);
                result.push_back({ extension, "*." + extension });
            }
            if (separator == StringView::npos)
                break;
            start = separator + 1;
        }
        return result;
    }
} // namespace Crowny
