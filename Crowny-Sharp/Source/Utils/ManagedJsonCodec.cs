using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Text;

namespace Crowny
{
    internal static class ManagedJsonCodec
    {
        internal static string Serialize(object value, bool prettyPrint)
        {
            StringBuilder output = new StringBuilder();
            HashSet<object> activeObjects = new HashSet<object>(ReferenceComparer.Instance);
            WriteValue(output, value, prettyPrint, 0, activeObjects);
            return output.ToString();
        }

        internal static object Deserialize(string json, Type type)
        {
            object value = Parse(json);
            return ConvertValue(value, type);
        }

        internal static object Parse(string json)
        {
            return new Parser(json).Parse();
        }

        internal static string SerializeDom(object value)
        {
            StringBuilder output = new StringBuilder();
            WriteDomValue(output, value);
            return output.ToString();
        }

        private static void WriteDomValue(StringBuilder output, object value)
        {
            if (value == null)
            {
                output.Append("null");
                return;
            }

            string text = value as string;
            if (text != null)
            {
                WriteString(output, text);
                return;
            }
            if (value is bool)
            {
                output.Append((bool)value ? "true" : "false");
                return;
            }
            Type type = value.GetType();
            if (IsNumber(type))
            {
                double floating = value is float || value is double ? Convert.ToDouble(value, CultureInfo.InvariantCulture) : 0.0;
                if ((value is float || value is double) && (double.IsNaN(floating) || double.IsInfinity(floating)))
                    throw new ArgumentException("JSON cannot represent NaN or infinity.");
                output.Append(Convert.ToString(value, CultureInfo.InvariantCulture));
                return;
            }

            Dictionary<string, object> members = value as Dictionary<string, object>;
            if (members != null)
            {
                output.Append('{');
                bool first = true;
                foreach (KeyValuePair<string, object> member in members)
                {
                    if (!first)
                        output.Append(',');
                    WriteString(output, member.Key);
                    output.Append(':');
                    WriteDomValue(output, member.Value);
                    first = false;
                }
                output.Append('}');
                return;
            }

            IList elements = value as IList;
            if (elements != null)
            {
                output.Append('[');
                for (int index = 0; index < elements.Count; ++index)
                {
                    if (index != 0)
                        output.Append(',');
                    WriteDomValue(output, elements[index]);
                }
                output.Append(']');
                return;
            }
            throw new ArgumentException("Unsupported JSON DOM value " + type.FullName + ".");
        }

        private static void WriteValue(StringBuilder output, object value, bool pretty, int depth, HashSet<object> activeObjects)
        {
            if (value == null)
            {
                output.Append("null");
                return;
            }

            Type type = value.GetType();
            if (type == typeof(string) || type == typeof(char))
            {
                WriteString(output, Convert.ToString(value, CultureInfo.InvariantCulture));
                return;
            }
            if (type == typeof(bool))
            {
                output.Append((bool)value ? "true" : "false");
                return;
            }
            if (type.IsEnum)
            {
                output.Append(Convert.ToInt64(value, CultureInfo.InvariantCulture).ToString(CultureInfo.InvariantCulture));
                return;
            }
            if (IsNumber(type))
            {
                double floating = value is float || value is double ? Convert.ToDouble(value, CultureInfo.InvariantCulture) : 0.0;
                if ((value is float || value is double) && (double.IsNaN(floating) || double.IsInfinity(floating)))
                    throw new ArgumentException("JSON cannot represent NaN or infinity.");
                output.Append(Convert.ToString(value, CultureInfo.InvariantCulture));
                return;
            }

            bool trackReference = !type.IsValueType;
            if (trackReference && !activeObjects.Add(value))
                throw new InvalidOperationException("JsonUtility cannot serialize cyclic object graphs.");
            try
            {
                IEnumerable sequence = value as IEnumerable;
                if (sequence != null)
                {
                    WriteArray(output, sequence, pretty, depth, activeObjects);
                    return;
                }
                WriteObject(output, value, pretty, depth, activeObjects);
            }
            finally
            {
                if (trackReference)
                    activeObjects.Remove(value);
            }
        }

        private static void WriteArray(StringBuilder output, IEnumerable sequence, bool pretty, int depth,
                                       HashSet<object> activeObjects)
        {
            output.Append('[');
            bool first = true;
            foreach (object item in sequence)
            {
                if (!first)
                    output.Append(',');
                WriteSeparator(output, pretty, depth + 1);
                WriteValue(output, item, pretty, depth + 1, activeObjects);
                first = false;
            }
            if (!first)
                WriteSeparator(output, pretty, depth);
            output.Append(']');
        }

        private static void WriteObject(StringBuilder output, object value, bool pretty, int depth,
                                        HashSet<object> activeObjects)
        {
            output.Append('{');
            FieldInfo[] fields = GetSerializableFields(value.GetType());
            for (int index = 0; index < fields.Length; ++index)
            {
                if (index != 0)
                    output.Append(',');
                WriteSeparator(output, pretty, depth + 1);
                WriteString(output, fields[index].Name);
                output.Append(pretty ? ": " : ":");
                WriteValue(output, fields[index].GetValue(value), pretty, depth + 1, activeObjects);
            }
            if (fields.Length != 0)
                WriteSeparator(output, pretty, depth);
            output.Append('}');
        }

        private static void WriteSeparator(StringBuilder output, bool pretty, int depth)
        {
            if (!pretty)
                return;
            output.AppendLine();
            output.Append(' ', depth * 2);
        }

        private static void WriteString(StringBuilder output, string value)
        {
            output.Append('"');
            foreach (char character in value ?? string.Empty)
            {
                switch (character)
                {
                case '"': output.Append("\\\""); break;
                case '\\': output.Append("\\\\"); break;
                case '\b': output.Append("\\b"); break;
                case '\f': output.Append("\\f"); break;
                case '\n': output.Append("\\n"); break;
                case '\r': output.Append("\\r"); break;
                case '\t': output.Append("\\t"); break;
                default:
                    if (character < ' ')
                        output.Append("\\u").Append(((int)character).ToString("x4", CultureInfo.InvariantCulture));
                    else
                        output.Append(character);
                    break;
                }
            }
            output.Append('"');
        }

        private static object ConvertValue(object value, Type type)
        {
            Type nullableType = Nullable.GetUnderlyingType(type);
            if (nullableType != null)
                return value == null ? null : ConvertValue(value, nullableType);
            if (value == null)
                return type.IsValueType ? Activator.CreateInstance(type) : null;
            if (type == typeof(object))
                return value;
            if (type == typeof(string))
                return Convert.ToString(value, CultureInfo.InvariantCulture);
            if (type == typeof(char))
            {
                string text = Convert.ToString(value, CultureInfo.InvariantCulture);
                return text.Length == 0 ? '\0' : text[0];
            }
            if (type == typeof(bool))
                return Convert.ToBoolean(value, CultureInfo.InvariantCulture);
            if (type.IsEnum)
                return Enum.ToObject(type, Convert.ChangeType(value, Enum.GetUnderlyingType(type), CultureInfo.InvariantCulture));
            if (IsNumber(type))
                return Convert.ChangeType(value, type, CultureInfo.InvariantCulture);

            IList input = value as IList;
            if (type.IsArray)
            {
                if (input == null)
                    throw new FormatException("Expected a JSON array for " + type.FullName + ".");
                Type elementType = type.GetElementType();
                Array result = Array.CreateInstance(elementType, input.Count);
                for (int index = 0; index < input.Count; ++index)
                    result.SetValue(ConvertValue(input[index], elementType), index);
                return result;
            }
            if (typeof(IList).IsAssignableFrom(type) && type.IsGenericType)
            {
                if (input == null)
                    throw new FormatException("Expected a JSON array for " + type.FullName + ".");
                Type elementType = type.GetGenericArguments()[0];
                Type concreteType = type.IsInterface || type.IsAbstract
                    ? typeof(List<>).MakeGenericType(elementType)
                    : type;
                IList result = (IList)Activator.CreateInstance(concreteType);
                foreach (object item in input)
                    result.Add(ConvertValue(item, elementType));
                return result;
            }

            Dictionary<string, object> members = value as Dictionary<string, object>;
            if (members == null)
                throw new FormatException("Expected a JSON object for " + type.FullName + ".");
            object instance = Activator.CreateInstance(type);
            foreach (FieldInfo field in GetSerializableFields(type))
            {
                object member;
                if (members.TryGetValue(field.Name, out member))
                    field.SetValue(instance, ConvertValue(member, field.FieldType));
            }
            return instance;
        }

        private static FieldInfo[] GetSerializableFields(Type type)
        {
            List<FieldInfo> result = new List<FieldInfo>();
            foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public))
            {
                if (!field.IsStatic && !Attribute.IsDefined(field, typeof(DontSerializeField)))
                    result.Add(field);
            }
            result.Sort((first, second) => first.MetadataToken.CompareTo(second.MetadataToken));
            return result.ToArray();
        }

        private static bool IsNumber(Type type)
        {
            return type == typeof(byte) || type == typeof(sbyte) || type == typeof(short) || type == typeof(ushort) ||
                   type == typeof(int) || type == typeof(uint) || type == typeof(long) || type == typeof(ulong) ||
                   type == typeof(float) || type == typeof(double) || type == typeof(decimal);
        }

        private sealed class ReferenceComparer : IEqualityComparer<object>
        {
            internal static readonly ReferenceComparer Instance = new ReferenceComparer();
            public new bool Equals(object first, object second) { return ReferenceEquals(first, second); }
            public int GetHashCode(object value) { return System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(value); }
        }

        private sealed class Parser
        {
            private readonly string input;
            private int position;

            internal Parser(string input) { this.input = input ?? string.Empty; }

            internal object Parse()
            {
                SkipWhitespace();
                object result = ParseValue();
                SkipWhitespace();
                if (position != input.Length)
                    throw Error("Unexpected trailing JSON content.");
                return result;
            }

            private object ParseValue()
            {
                SkipWhitespace();
                if (position >= input.Length)
                    throw Error("Unexpected end of JSON.");
                char token = input[position];
                if (token == '{') return ParseObject();
                if (token == '[') return ParseArray();
                if (token == '"') return ParseString();
                if (token == 't') { Expect("true"); return true; }
                if (token == 'f') { Expect("false"); return false; }
                if (token == 'n') { Expect("null"); return null; }
                if (token == '-' || (token >= '0' && token <= '9')) return ParseNumber();
                throw Error("Unexpected JSON token.");
            }

            private Dictionary<string, object> ParseObject()
            {
                Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
                ++position;
                SkipWhitespace();
                if (Consume('}')) return result;
                while (true)
                {
                    SkipWhitespace();
                    if (position >= input.Length || input[position] != '"') throw Error("Expected an object member name.");
                    string name = ParseString();
                    SkipWhitespace();
                    if (!Consume(':')) throw Error("Expected ':' after an object member name.");
                    result[name] = ParseValue();
                    SkipWhitespace();
                    if (Consume('}')) return result;
                    if (!Consume(',')) throw Error("Expected ',' between object members.");
                }
            }

            private List<object> ParseArray()
            {
                List<object> result = new List<object>();
                ++position;
                SkipWhitespace();
                if (Consume(']')) return result;
                while (true)
                {
                    result.Add(ParseValue());
                    SkipWhitespace();
                    if (Consume(']')) return result;
                    if (!Consume(',')) throw Error("Expected ',' between array values.");
                }
            }

            private string ParseString()
            {
                ++position;
                StringBuilder result = new StringBuilder();
                while (position < input.Length)
                {
                    char character = input[position++];
                    if (character == '"') return result.ToString();
                    if (character != '\\')
                    {
                        if (character < ' ') throw Error("Control characters must be escaped in JSON strings.");
                        result.Append(character);
                        continue;
                    }
                    if (position >= input.Length) throw Error("Incomplete JSON escape sequence.");
                    char escape = input[position++];
                    switch (escape)
                    {
                    case '"': result.Append('"'); break;
                    case '\\': result.Append('\\'); break;
                    case '/': result.Append('/'); break;
                    case 'b': result.Append('\b'); break;
                    case 'f': result.Append('\f'); break;
                    case 'n': result.Append('\n'); break;
                    case 'r': result.Append('\r'); break;
                    case 't': result.Append('\t'); break;
                    case 'u': result.Append(ParseUnicodeEscape()); break;
                    default: throw Error("Invalid JSON escape sequence.");
                    }
                }
                throw Error("Unterminated JSON string.");
            }

            private char ParseUnicodeEscape()
            {
                if (position + 4 > input.Length) throw Error("Incomplete Unicode escape.");
                int value;
                if (!int.TryParse(input.Substring(position, 4), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value))
                    throw Error("Invalid Unicode escape.");
                position += 4;
                return (char)value;
            }

            private object ParseNumber()
            {
                int start = position;
                if (Consume('-')) { }
                if (Consume('0')) { }
                else
                {
                    if (!ConsumeDigits()) throw Error("Invalid JSON number.");
                }
                bool floating = false;
                if (Consume('.'))
                {
                    floating = true;
                    if (!ConsumeDigits()) throw Error("Invalid JSON fraction.");
                }
                if (position < input.Length && (input[position] == 'e' || input[position] == 'E'))
                {
                    floating = true;
                    ++position;
                    if (position < input.Length && (input[position] == '+' || input[position] == '-')) ++position;
                    if (!ConsumeDigits()) throw Error("Invalid JSON exponent.");
                }
                string token = input.Substring(start, position - start);
                if (floating)
                    return double.Parse(token, NumberStyles.Float, CultureInfo.InvariantCulture);
                long signed;
                if (long.TryParse(token, NumberStyles.Integer, CultureInfo.InvariantCulture, out signed)) return signed;
                return ulong.Parse(token, NumberStyles.Integer, CultureInfo.InvariantCulture);
            }

            private bool ConsumeDigits()
            {
                int start = position;
                while (position < input.Length && input[position] >= '0' && input[position] <= '9') ++position;
                return position != start;
            }

            private void Expect(string token)
            {
                if (position + token.Length > input.Length || string.CompareOrdinal(input, position, token, 0, token.Length) != 0)
                    throw Error("Invalid JSON literal.");
                position += token.Length;
            }

            private bool Consume(char token)
            {
                if (position >= input.Length || input[position] != token) return false;
                ++position;
                return true;
            }

            private void SkipWhitespace()
            {
                while (position < input.Length && char.IsWhiteSpace(input[position])) ++position;
            }

            private FormatException Error(string message)
            {
                return new FormatException(message + " Position " + position.ToString(CultureInfo.InvariantCulture) + ".");
            }
        }
    }
}
