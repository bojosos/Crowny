using System;
using System.Runtime.CompilerServices;
using System.Globalization;

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

        private static string GetString(object message)
        {
            if (message == null)
                return "Null";
            var formattable = message as IFormattable;
            if (formattable != null)
                return formattable.ToString(null, CultureInfo.InvariantCulture);
            else
                return message.ToString();
        }

        /// <summary>
        /// Logs a message to the console
        /// </summary>
        /// <param name="logType">The level of the debug message.</param>
        /// <param name="format">Format.</param>
        /// <param name="args">Arguments to format.</param>
        public static void LogFormat(LogType logType, string format, params object[] args)
        {
            switch (logType)
            {
                case LogType.Info:       Internal_Log(string.Format(format, args)); break;
                case LogType.Warning:    Internal_LogWarning(string.Format(format, args)); break;
                case LogType.Error:      Internal_LogError(string.Format(format, args)); break;
                case LogType.Exception:  Internal_LogException(string.Format(format, args)); break;
            }
        }

        /// <summary>
        /// Logs and informational message to the console.
        /// </summary>
        /// <param name="message">Message.</param>
        public static void Log(object message)
        {
            Internal_Log(GetString(message));
        }

        /// <summary>
        /// Logs a warning message to the console.
        /// </summary>
        /// <param name="message">Message</param>
        public static void LogWarning(object message)
        {
            Internal_LogWarning(GetString(message));
        }

        /// <summary>
        /// Logs an error message to the console.
        /// </summary>
        /// <param name="message">Message.</param>
        public static void LogError(object message)
        {
            Internal_LogError(GetString(message));
        }

        /// <summary>
        /// Logs an exception to the console.
        /// </summary>
        /// <param name="message">Message.</param>
        public static void LogException(object message)
        {
            Internal_LogException(GetString(message));
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_Log(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_LogWarning(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_LogError(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_LogException(string message);
    }
}
