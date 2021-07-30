#include "cwpch.h"

#include "Crowny/Common/PlatformUtils.h"

#include <uuid/uuid.h>

namespace Crowny
{
    Uuid PlatformUtils::GenerateUuid()
    {
        uuid_t native;
        uuid_generate(native);

        return Uuid(*(uint32_t*)&native[0], *(uint32_t*)&native[4], *(uint32_t*)&native[8], *(uint32_t*)&native[12]);
    }
    
    void PlatformUtils::ShowInExplorer(const std::string& filepath)
    {
        const char* cmdPatter = "nautilus '%s'";
        char* cmdStr = new char[filepath.size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());
        
        if (system(cmdStr)) {};
        delete[] cmdStr;
    }
    
    void PlatformUtils::OpenExternally(const std::string& filepath)
    {
        const char* cmdPatter = "xdg-open '%s'";
        char* cmdStr = new char[filepath.size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());
        
        if (system(cmdStr)) {};
        delete[] cmdStr;
    }

}