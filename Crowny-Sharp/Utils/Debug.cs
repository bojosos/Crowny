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
        public static void Log(string message)
        {
            Internal_Log(message);
        }

        public static void LogWarning(string message)
        {
            Internal_LogWarning(message);
        }

        public static void LogError(string message)
        {
            Internal_LogError(message);
        }

        public static void LogException(string message)
        {
            Internal_LogException(message);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Internal_Log(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Internal_LogWarning(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Internal_LogError(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Internal_LogException(string message);
    }
}
