using System;
using System.Runtime.CompilerServices;
using System.Globalization;

namespace Crowny
{

    public static class JsonUtility
    {
        /// <summary>
        /// Generate a JSON representation of the public fields of the object.
        /// </summary>
        /// <param name="obj">The object to convert to JSON.</param>
        /// <param name="prettyPrint">If true, format the output for readibility. If false, format the output for minumum size. Default is true.</param>
        /// <returns>The resulting JSON string.</returns>
        public static string ToJson(object obj, bool prettyPrint=true)
        {
            if (obj == null)
                return "";
            if (obj is ScriptObject && !(obj is EntityBehaviour))
                throw new ArgumentException("JsonUtility.ToJson does not support engine types.");
            return Internal_ToJson(obj, prettyPrint);
        }

        public static T FromJson<T>(string json) { return (T)FromJson(json, typeof(T)); }

        public static object FromJson(string json, Type type)
        {
            if (string.IsNullOrEmpty(json))
                return null;
            if (type == null)
                throw new ArgumentNullException("JSON type cannot be null");
            if (type.IsAbstract || type.IsSubclassOf(typeof(Crowny.ScriptObject)))
                throw new ArgumentException("Cannot deserialize JSON to new instance of type '" + type.Name + "'.");
            return Internal_FromJson(json, type);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static string Internal_ToJson(object obj, bool prettyPrint);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static object Internal_FromJson(string message, Type type);

    }
}
