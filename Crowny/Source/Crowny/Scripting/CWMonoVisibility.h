#pragma once

namespace Crowny
{

    enum class CWMonoVisibility
    {
        Private,
        Protected,
        Internal,
        ProtectedInternal,
        Public
    };

    static String CWMonoVisibilityToString(CWMonoVisibility visibility)
    {
        switch (visibility)
        {
        case Crowny::CWMonoVisibility::Private:
            return "private";
        case Crowny::CWMonoVisibility::ProtectedInternal:
            return "protected internal";
        case Crowny::CWMonoVisibility::Internal:
            return "internal";
        case Crowny::CWMonoVisibility::Protected:
            return "protected";
        case Crowny::CWMonoVisibility::Public:
            return "public";
        }

        CW_ENGINE_ERROR("Unknown mono visibility!");
        return "";
    }
} // namespace Crowny