using System;

namespace Crowny
{

    public static class ScriptUtils
    {
        public static string[] GetEnumNames(Type type)
        {
            return Enum.GetNames(type);
        }
    }
}