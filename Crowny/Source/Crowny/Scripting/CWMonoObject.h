#pragma once

#include "testttt.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/object.h>
END_MONO_INCLUDE

namespace Crowny
{
	class CWMonoObject
	{
	public:
		CWMonoObject(MonoObject* monoObj);
		 
		bool Valid() const;
	private:
		MonoObject* m_Object;
	};
}