#include "cwpch.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"

namespace Crowny
{
    char const* const RenderCapabilities::GPU_VENDOR_STRINGS[GPU_VENDOR_COUNT] = { "unknown", "nvidia", "amd",
                                                                                   "intel" };

    GPUVendor RenderCapabilities::VendorFromString(const String& vendorString)
    {
        GPUVendor ret = GPU_UNKNOWN;
        String str = vendorString;
        // TODO: to lower case
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

    String RenderCapabilities::VendorToString(GPUVendor vendor) { return GPU_VENDOR_STRINGS[vendor]; }
} // namespace Crowny