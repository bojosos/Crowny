using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;

namespace Crowny
{
    internal static class ManagedStateCodec
    {
        internal static string Capture(object instance)
        {
            if (instance == null)
                throw new ArgumentNullException("instance");
            Type type = instance.GetType();
            return Capture(type, instance, ScriptMetadata.Discover(type, true));
        }

        internal static string Capture(Type type, object instance, IReadOnlyList<ScriptMember> members)
        {
            Dictionary<string, object> root = new Dictionary<string, object>(StringComparer.Ordinal);
            root.Add("Assembly", type.Assembly.GetName().Name);
            root.Add("Namespace", type.Namespace ?? string.Empty);
            root.Add("TypeName", GetTypeName(type));
            Dictionary<string, object> fields = new Dictionary<string, object>(StringComparer.Ordinal);
            Dictionary<string, object> kinds = new Dictionary<string, object>(StringComparer.Ordinal);
            root.Add("Kinds", kinds);
            root.Add("Fields", fields);

            HashSet<object> visited = new HashSet<object>(ReferenceComparer.Instance);
            foreach (ScriptMember member in members)
            {
                if (!member.IsSerializable)
                    continue;
                kinds.Add(member.Name, ScriptMetadata.ValueKind(member.ValueType));
                fields.Add(member.Name, WriteValue(member.GetValue(instance), member.ValueType, visited));
            }
            return ManagedJsonCodec.SerializeDom(root);
        }

        internal static void Apply(object instance, string state)
        {
            if (instance == null)
                throw new ArgumentNullException("instance");
            Type type = instance.GetType();
            Apply(instance, ScriptMetadata.Discover(type, true), state);
        }

        internal static void Apply(object instance, IReadOnlyList<ScriptMember> members, string state)
        {
            Dictionary<string, object> root = RequireObject(ManagedJsonCodec.Parse(state), "managed state");
            object fieldsValue;
            Dictionary<string, object> fields = root.TryGetValue("Fields", out fieldsValue)
                ? RequireObject(fieldsValue, "managed state Fields")
                : root;
            foreach (ScriptMember member in members)
            {
                if (!member.IsSerializable || !member.CanWrite)
                    continue;
                object memberValue;
                if (!fields.TryGetValue(member.Name, out memberValue))
                {
                    bool found = false;
                    foreach (string formerName in member.FormerNames)
                    {
                        if (fields.TryGetValue(formerName, out memberValue))
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        continue;
                }
                member.SetValue(instance, ReadValue(memberValue, member.ValueType));
            }
        }

        internal static string TryApply(object instance, string state)
        {
            string previous;
            try
            {
                previous = Capture(instance);
            }
            catch (Exception error)
            {
                return "Could not capture managed state before applying it: " + error;
            }

            try
            {
                Apply(instance, state);
                return null;
            }
            catch (Exception applyError)
            {
                try
                {
                    Apply(instance, previous);
                }
                catch (Exception rollbackError)
                {
                    return "Managed state application and rollback both failed. Apply error: " + applyError +
                           " Rollback error: " + rollbackError;
                }
                return applyError.ToString();
            }
        }

        private static object WriteValue(object value, Type declaredType, HashSet<object> visited)
        {
            Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
            if (value == null)
                return null;
            if (type == typeof(bool) || type == typeof(string))
                return value;
            if (type == typeof(char))
                return value.ToString();
            if (type.IsEnum)
            {
                Type underlyingType = Enum.GetUnderlyingType(type);
                return IsUnsignedInteger(underlyingType)
                    ? (object)Convert.ToUInt64(value, CultureInfo.InvariantCulture)
                    : Convert.ToInt64(value, CultureInfo.InvariantCulture);
            }
            if (type == typeof(decimal))
                return ((decimal)value).ToString(CultureInfo.InvariantCulture);
            if (IsNumber(type))
                return value;
            if (type == typeof(UUID))
                return value.ToString();
            Entity entity = value as Entity;
            if (entity != null)
                return entity.uuid.ToString();
            Component component = value as Component;
            if (component != null)
                return component.m_ManagedEntityId.ToString();
            Asset asset = value as Asset;
            if (asset != null)
                return asset.uuid.ToString();
            if (type == typeof(Vector2))
            {
                Vector2 vector = (Vector2)value;
                return Values(vector.x, vector.y);
            }
            if (type == typeof(Vector3))
            {
                Vector3 vector = (Vector3)value;
                return Values(vector.x, vector.y, vector.z);
            }
            if (type == typeof(Vector4))
            {
                Vector4 vector = (Vector4)value;
                return Values(vector.x, vector.y, vector.z, vector.w);
            }
            if (type == typeof(Color))
            {
                Color color = (Color)value;
                return Values(color.r, color.g, color.b, color.a);
            }
            if (type == typeof(Quaternion))
            {
                Quaternion quaternion = (Quaternion)value;
                return Values(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
            }
            if (type == typeof(Matrix4))
            {
                Matrix4 matrix = (Matrix4)value;
                List<object> values = new List<object>(16);
                for (int index = 0; index < 16; ++index)
                    values.Add(matrix[index]);
                return values;
            }

            bool tracked = !type.IsValueType;
            if (tracked && !visited.Add(value))
                throw new InvalidOperationException("Managed state contains a reference cycle at " + type.FullName + ".");
            try
            {
                if (type.IsArray)
                {
                    Type elementType = type.GetElementType();
                    List<object> elements = new List<object>();
                    foreach (object element in (Array)value)
                        elements.Add(WriteValue(element, elementType, visited));
                    return elements;
                }
                if (IsGeneric(type, typeof(List<>)))
                {
                    Type elementType = type.GetGenericArguments()[0];
                    List<object> elements = new List<object>();
                    foreach (object element in (IEnumerable)value)
                        elements.Add(WriteValue(element, elementType, visited));
                    return elements;
                }
                if (IsGeneric(type, typeof(Dictionary<,>)))
                {
                    Type[] arguments = type.GetGenericArguments();
                    List<object> entries = new List<object>();
                    foreach (DictionaryEntry entry in (IDictionary)value)
                    {
                        Dictionary<string, object> encoded = new Dictionary<string, object>(StringComparer.Ordinal);
                        encoded.Add("Key", WriteValue(entry.Key, arguments[0], visited));
                        encoded.Add("Value", WriteValue(entry.Value, arguments[1], visited));
                        entries.Add(encoded);
                    }
                    return entries;
                }

                Dictionary<string, object> members = new Dictionary<string, object>(StringComparer.Ordinal);
                foreach (ScriptMember member in ScriptMetadata.Discover(type, false))
                    if (member.IsSerializable)
                        members.Add(member.Name, WriteValue(member.GetValue(value), member.ValueType, visited));
                return members;
            }
            finally
            {
                if (tracked)
                    visited.Remove(value);
            }
        }

        private static object ReadValue(object value, Type declaredType)
        {
            Type nullable = Nullable.GetUnderlyingType(declaredType);
            Type type = nullable ?? declaredType;
            if (value == null)
                return nullable != null || !type.IsValueType ? null : Activator.CreateInstance(type);
            if (type == typeof(bool))
                return Convert.ToBoolean(value, CultureInfo.InvariantCulture);
            if (type == typeof(string))
                return Convert.ToString(value, CultureInfo.InvariantCulture);
            if (type == typeof(char))
            {
                string text = Convert.ToString(value, CultureInfo.InvariantCulture);
                return text.Length == 0 ? '\0' : text[0];
            }
            if (type.IsEnum)
                return Enum.ToObject(type, Convert.ChangeType(value, Enum.GetUnderlyingType(type), CultureInfo.InvariantCulture));
            if (IsNumber(type))
                return Convert.ChangeType(value, type, CultureInfo.InvariantCulture);
            if (type == typeof(UUID))
                return ParseUuid(Convert.ToString(value, CultureInfo.InvariantCulture));
            if (typeof(Entity).IsAssignableFrom(type))
            {
                Entity entity = (Entity)Activator.CreateInstance(type, true);
                entity.m_ManagedUuid = ParseUuid(Convert.ToString(value, CultureInfo.InvariantCulture));
                return entity;
            }
            if (typeof(Component).IsAssignableFrom(type))
                return ManagedRuntimeContext.GetComponent(ParseUuid(Convert.ToString(value, CultureInfo.InvariantCulture)), type);
            if (typeof(Asset).IsAssignableFrom(type))
                return ManagedRuntimeContext.CreateAsset(type, ParseUuid(Convert.ToString(value, CultureInfo.InvariantCulture)));
            if (type == typeof(Vector2))
            {
                float[] elements = ReadNumbers(value, 2);
                return new Vector2(elements[0], elements[1]);
            }
            if (type == typeof(Vector3))
            {
                float[] elements = ReadNumbers(value, 3);
                return new Vector3(elements[0], elements[1], elements[2]);
            }
            if (type == typeof(Vector4))
            {
                float[] elements = ReadNumbers(value, 4);
                return new Vector4(elements[0], elements[1], elements[2], elements[3]);
            }
            if (type == typeof(Color))
            {
                float[] elements = ReadNumbers(value, 4);
                return new Color(elements[0], elements[1], elements[2], elements[3]);
            }
            if (type == typeof(Quaternion))
            {
                float[] elements = ReadNumbers(value, 4);
                return new Quaternion(elements[0], elements[1], elements[2], elements[3]);
            }
            if (type == typeof(Matrix4))
            {
                float[] elements = ReadNumbers(value, 16);
                Matrix4 matrix = new Matrix4();
                for (int index = 0; index < 16; ++index)
                    matrix[index] = elements[index];
                return matrix;
            }

            IList input = value as IList;
            if (type.IsArray)
            {
                RequireArray(input, type);
                Type elementType = type.GetElementType();
                Array array = Array.CreateInstance(elementType, input.Count);
                for (int index = 0; index < input.Count; ++index)
                    array.SetValue(ReadValue(input[index], elementType), index);
                return array;
            }
            if (IsGeneric(type, typeof(List<>)))
            {
                RequireArray(input, type);
                Type elementType = type.GetGenericArguments()[0];
                IList list = (IList)Activator.CreateInstance(type);
                foreach (object element in input)
                    list.Add(ReadValue(element, elementType));
                return list;
            }
            if (IsGeneric(type, typeof(Dictionary<,>)))
            {
                RequireArray(input, type);
                Type[] arguments = type.GetGenericArguments();
                IDictionary dictionary = (IDictionary)Activator.CreateInstance(type);
                foreach (object element in input)
                {
                    Dictionary<string, object> entry = RequireObject(element, "dictionary entry");
                    object keyValue;
                    object entryValue;
                    if (!entry.TryGetValue("Key", out keyValue) || !entry.TryGetValue("Value", out entryValue))
                        throw new FormatException("Expected Key and Value members in a dictionary entry for " + type.FullName + ".");
                    object key = ReadValue(keyValue, arguments[0]);
                    if (key == null)
                        throw new FormatException("Dictionary key for " + type.FullName + " cannot be null.");
                    dictionary.Add(key, ReadValue(entryValue, arguments[1]));
                }
                return dictionary;
            }

            Dictionary<string, object> encodedMembers = RequireObject(value, type.FullName);
            object instance = Activator.CreateInstance(type, true);
            foreach (ScriptMember member in ScriptMetadata.Discover(type, false))
            {
                if (!member.IsSerializable || !member.CanWrite)
                    continue;
                object memberValue;
                if (!TryGetMember(encodedMembers, member, out memberValue))
                    continue;
                member.SetValue(instance, ReadValue(memberValue, member.ValueType));
            }
            return instance;
        }

        private static bool TryGetMember(Dictionary<string, object> members, ScriptMember member, out object value)
        {
            if (members.TryGetValue(member.Name, out value))
                return true;
            foreach (string formerName in member.FormerNames)
            {
                if (members.TryGetValue(formerName, out value))
                    return true;
            }
            value = null;
            return false;
        }

        private static List<object> Values(params float[] values)
        {
            List<object> result = new List<object>(values.Length);
            foreach (float value in values)
                result.Add(value);
            return result;
        }

        private static float[] ReadNumbers(object value, int count)
        {
            IList elements = value as IList;
            if (elements == null || elements.Count < count)
                throw new FormatException("Expected an array with at least " + count + " numeric values.");
            float[] result = new float[count];
            for (int index = 0; index < count; ++index)
                result[index] = Convert.ToSingle(elements[index], CultureInfo.InvariantCulture);
            return result;
        }

        private static Dictionary<string, object> RequireObject(object value, string description)
        {
            Dictionary<string, object> result = value as Dictionary<string, object>;
            if (result == null)
                throw new FormatException("Expected a JSON object for " + description + ".");
            return result;
        }

        private static void RequireArray(IList value, Type type)
        {
            if (value == null)
                throw new FormatException("Expected a JSON array for " + type.FullName + ".");
        }

        private static bool IsNumber(Type type)
        {
            return IsUnsignedInteger(type) || type == typeof(sbyte) || type == typeof(short) || type == typeof(int) ||
                   type == typeof(long) || type == typeof(float) || type == typeof(double) || type == typeof(decimal);
        }

        private static bool IsUnsignedInteger(Type type)
        {
            return type == typeof(byte) || type == typeof(ushort) || type == typeof(uint) || type == typeof(ulong);
        }

        private static bool IsGeneric(Type type, Type definition)
        {
            return type.IsGenericType && type.GetGenericTypeDefinition() == definition;
        }

        private static UUID ParseUuid(string value)
        {
            string text = (value ?? string.Empty).Replace("-", string.Empty);
            if (text.Length != 32)
                throw new FormatException("Expected a 128-bit UUID string.");
            return new UUID(Convert.ToUInt32(text.Substring(0, 8), 16), Convert.ToUInt32(text.Substring(8, 8), 16),
                            Convert.ToUInt32(text.Substring(16, 8), 16), Convert.ToUInt32(text.Substring(24, 8), 16));
        }

        private static string GetTypeName(Type type)
        {
            string fullName = type.FullName ?? type.Name;
            string typeNamespace = type.Namespace ?? string.Empty;
            return typeNamespace.Length == 0 ? fullName : fullName.Substring(typeNamespace.Length + 1);
        }

        private sealed class ReferenceComparer : IEqualityComparer<object>
        {
            internal static readonly ReferenceComparer Instance = new ReferenceComparer();
            public new bool Equals(object first, object second) { return ReferenceEquals(first, second); }
            public int GetHashCode(object value)
            {
                return System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(value);
            }
        }
    }
}
