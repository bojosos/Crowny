#include "cwpch.h"

#include "Crowny/Renderer/RenderCapabilities.h"

namespace Crowny
{
    char const * const RenderCapabilities::GPU_VENDOR_STRINGS[GPU_VENDOR_COUNT] = 
    {
        "unknown",
        "nvidia",
        "amd",
        "intel"
    };

    GPUVendor RenderCapabilities::VendorFromString(const std::string& vendorString)
    {
        GPUVendor ret = GPU_UNKNOWN;
        std::string str = vendorString;
        //TODO: to lower case
        for (int i = 0; i < GPU_VENDOR_COUNT; i++)
        {
            if (GPU_VENDOR_STRINGS[i] == vendorString)
            {
                ret = static_cast<GPUVendor>(i);
                break;
            }
        }

        return ret;
    }

    std::string RenderCapabilities::VendorToString(GPUVendor vendor)
    {
        return GPU_VENDOR_STRINGS[vendor];
    }
}