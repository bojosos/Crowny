using System;
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
                case LogType.Info:       ManagedRuntimeContext.DebugWriteLog((int)LogType.Info, string.Format(format, args)); break;
                case LogType.Warning:    ManagedRuntimeContext.DebugWriteLog((int)LogType.Warning, string.Format(format, args)); break;
                case LogType.Error:      ManagedRuntimeContext.DebugWriteLog((int)LogType.Error, string.Format(format, args)); break;
                case LogType.Exception:  ManagedRuntimeContext.DebugWriteLog((int)LogType.Exception, string.Format(format, args)); break;
            }
        }

        /// <summary>
        /// Logs and informational message to the console.
        /// </summary>
        /// <param name="message">Message.</param>
        public static void Log(object message)
        {
            ManagedRuntimeContext.DebugWriteLog((int)LogType.Info, GetString(message));
        }

        /// <summary>
        /// Logs a warning message to the console.
        /// </summary>
        /// <param name="message">Message</param>
        public static void Warn(object message)
        {
            ManagedRuntimeContext.DebugWriteLog((int)LogType.Warning, GetString(message));
        }

        /// <summary>
        /// Logs an error message to the console.
        /// </summary>
        /// <param name="message">Message.</param>
        public static void Error(object message)
        {
            ManagedRuntimeContext.DebugWriteLog((int)LogType.Error, GetString(message));
        }

        /// <summary>
        /// Logs an exception to the console.
        /// </summary>
        /// <param name="message">Message.</param>
        public static void Exception(object message)
        {
            ManagedRuntimeContext.DebugWriteLog((int)LogType.Exception, GetString(message));
        }

    }
}
