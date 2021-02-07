#pragma once

#include <mono/metadata/metadata.h>

namespace Crowny
{
  class ScriptGameObject 
  {
  public:
    static void InitRuntimeFunctions();

  private:
    static MonoObject* Internal_FindObject(MonoString* name);
  };

}
