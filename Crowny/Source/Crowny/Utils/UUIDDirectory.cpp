#include "cwpch.h"

#include "Crowny/Utils/UUIDDirectory.h"

namespace Crowny
{

    UUIDDirectory::UUIDDirectory(const Path& path) : m_Path(path)
    {
        if (!fs::is_directory(path))
            fs::create_directories(path);
        Refresh();
    }

    void UUIDDirectory::Refresh()
    {
        for (auto entry : fs::recursive_directory_iterator(m_Path))
        {
            String filename = entry.path().filename().string();
            if (filename.size() == 36)
                m_Uuids[UUID42(filename)] = entry.path();
        }
    }

    Path UUIDDirectory::GetPath(const UUID42& uuid) const
    {
        auto iterFind = m_Uuids.find(uuid);
        if (iterFind != m_Uuids.end())
            return iterFind->second;
        String uuidStr = uuid.ToString();
        Path directory = m_Path / (uuidStr.substr(0, 2));
        Path path = directory / uuidStr;
        if (!fs::is_directory(directory))
            fs::create_directories(directory);
        return path;
    }

    void UUIDDirectory::RemovePath(const UUID42& uuid)
    {
        auto iterFind = m_Uuids.find(uuid);
        if (iterFind != m_Uuids.end())
            m_Uuids.erase(iterFind);
    }

    void UUIDDirectory::RemovePath(const Path& path)
    {
        String filename = path.filename().string();
        if (filename.size() == 36)
        {
            UUID42 uuid(filename);
            RemovePath(uuid);
        }
    }

    void UUIDDirectory::OnFileWrite(const UUID42& uuid)
    {
        if (m_Uuids.find(uuid) != m_Uuids.end())
            return;
        String uuidStr = uuid.ToString();
        Path path = m_Path / (uuidStr.substr(0, 2)) / uuidStr;
        m_Uuids[uuid] = path;
    }
} // namespace Crowny