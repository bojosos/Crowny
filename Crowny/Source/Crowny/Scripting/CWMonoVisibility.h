#pragma once

namespace Crowny
{

	enum class CWMonoVisibility
	{
		PRIVATE, PROTECTED_INTERNAL, INTERNAL, PROTECTED, PUBLIC
	};

	static std::string CWMonoVisibilityToString(CWMonoVisibility visibility)
	{
		switch (visibility)
		{
		case Crowny::CWMonoVisibility::PRIVATE:				return "private";
		case Crowny::CWMonoVisibility::PROTECTED_INTERNAL:  return "protected internal";
		case Crowny::CWMonoVisibility::INTERNAL:            return "internal";
		case Crowny::CWMonoVisibility::PROTECTED:           return "protected";
		case Crowny::CWMonoVisibility::PUBLIC:              return "public";
		}

		CW_ENGINE_ERROR("Unknown mono visibility!");
		return "";
	}
}