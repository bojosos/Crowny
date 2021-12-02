#pragma once

namespace Crowny
{

    enum class CrownyMonoVisibility
    {
        Private,
        Protected,
        Internal,
        ProtectedInternal,
        Public
    };

    static String CrownyMonoVisibilityToString(CrownyMonoVisibility visibility)
    {
        switch (visibility)
        {
        case Crowny::CrownyMonoVisibility::Private:
            return "private";
        case Crowny::CrownyMonoVisibility::ProtectedInternal:
            return "protected internal";
        case Crowny::CrownyMonoVisibility::Internal:
            return "internal";
        case Crowny::CrownyMonoVisibility::Protected:
            return "protected";
        case Crowny::CrownyMonoVisibility::Public:
            return "public";
        }

        CW_ENGINE_ERROR("Unknown mono visibility!");
        return "";
    }
} // namespace Crowny