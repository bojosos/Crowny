using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace Crowny
{
    internal static class ManagedButtonInvoker
    {
        private sealed class MethodMap
        {
            internal MethodMap(Type type)
            {
                Methods = new Dictionary<ulong, ScriptButtonMethod>();
                foreach (ScriptButtonMethod method in ScriptButtonMetadata.Discover(type))
                    Methods.Add(method.StableId, method);
                foreach (ScriptMember member in ScriptMetadata.Discover(type, true))
                {
                    foreach (ScriptValueChangedDescriptor action in member.ValueChangedActions)
                    {
                        if (!Methods.ContainsKey(action.StableId))
                            Methods.Add(action.StableId, new ScriptButtonMethod(action.Method, new Button(), action.StableId));
                    }
                }
            }

            internal Dictionary<ulong, ScriptButtonMethod> Methods { get; private set; }
        }

        private static readonly ConditionalWeakTable<Type, MethodMap> cache = new ConditionalWeakTable<Type, MethodMap>();

        internal static string Invoke(object instance, ulong methodId, string argumentsJson)
        {
            if (instance == null)
                throw new ArgumentNullException("instance");
            MethodMap methods = cache.GetValue(instance.GetType(), type => new MethodMap(type));
            ScriptButtonMethod button;
            if (!methods.Methods.TryGetValue(methodId, out button))
                throw new MissingMethodException(instance.GetType().FullName, "Inspector button " + methodId.ToString(CultureInfo.InvariantCulture));

            IList encodedArguments = ManagedJsonCodec.Parse(argumentsJson) as IList;
            if (encodedArguments == null || encodedArguments.Count != button.Parameters.Length)
                throw new FormatException("Inspector button arguments do not match " + button.Method.Name + ".");
            object[] arguments = new object[button.Parameters.Length];
            for (int index = 0; index < arguments.Length; ++index)
                arguments[index] = ManagedStateCodec.ReadInspectorValue(encodedArguments[index], button.Parameters[index].ParameterType);

            object result = button.Method.Invoke(button.Method.IsStatic ? null : instance, arguments);
            Dictionary<string, object> encoded = new Dictionary<string, object>(StringComparer.Ordinal);
            bool hasResult = button.Method.ReturnType != typeof(void);
            encoded.Add("HasResult", hasResult);
            if (hasResult && ScriptButtonMetadata.IsSupportedValue(button.Method.ReturnType))
            {
                encoded.Add("ResultKind", ScriptMetadata.ValueKind(button.Method.ReturnType));
                encoded.Add("Result", ManagedStateCodec.EncodeInspectorValue(result, button.Method.ReturnType));
            }
            else if (hasResult)
            {
                encoded.Add("ResultKind", "String");
                encoded.Add("Result", result == null ? "null" : Convert.ToString(result, CultureInfo.InvariantCulture));
            }
            else
            {
                encoded.Add("ResultKind", "Null");
                encoded.Add("Result", null);
            }
            return ManagedJsonCodec.SerializeDom(encoded);
        }
    }
}
