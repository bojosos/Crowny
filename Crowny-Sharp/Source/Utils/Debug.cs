using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public enum LogType
    {
        // LogType used for regular log messages.
        Info = 0,
        // LogType used for Warnings.
        Warning = 1,
        // LogType used for Errors.
        Error = 2,
        // LogType used for Exceptions.
        Exception = 3
    }

    public class Debug
    {

        public static void LogFormat(LogType logType, string format, params object[] args)
        {
            switch (logType)
            {
                case LogType.Info:       Log(string.Format(format, args)); break;
                case LogType.Warning:    LogWarning(string.Format(format, args)); break;
                case LogType.Error:      LogError(string.Format(format, args)); break;
                case LogType.Exception:  LogException(string.Format(format, args)); break;
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static void Log(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static void LogWarning(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static void LogError(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern static void LogException(string message);
    }
}
