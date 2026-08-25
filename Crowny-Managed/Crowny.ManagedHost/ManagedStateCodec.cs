using System.Collections;
using System.Text;
using System.Text.Json;

namespace Crowny.ManagedHost;

internal static class ManagedStateCodec
{
    internal static byte[] Capture(Type type, object instance, IReadOnlyList<ScriptMember> members)
    {
        using var stream = new MemoryStream();
        using (var writer = new Utf8JsonWriter(stream))
        {
            writer.WriteStartObject();
            writer.WriteString("Assembly", type.Assembly.GetName().Name);
            writer.WriteString("Namespace", type.Namespace ?? string.Empty);
            writer.WriteString("TypeName", GetTypeName(type));
            writer.WritePropertyName("Fields");
            writer.WriteStartObject();
            var visited = new HashSet<object>(ReferenceEqualityComparer.Instance);
            foreach (ScriptMember member in members)
            {
                writer.WritePropertyName(member.Name);
                WriteValue(writer, member.GetValue(instance), member.ValueType, visited);
            }
            writer.WriteEndObject();
            writer.WriteEndObject();
        }
        return stream.ToArray();
    }

    internal static void Apply(object instance, IReadOnlyList<ScriptMember> members, ReadOnlySpan<byte> state)
    {
        var reader = new Utf8JsonReader(state);
        using JsonDocument document = JsonDocument.ParseValue(ref reader);
        JsonElement fields = document.RootElement.TryGetProperty("Fields", out JsonElement value) ? value : document.RootElement;
        foreach (ScriptMember member in members)
        {
            if (member.IsReadOnly)
                continue;
            JsonElement memberValue;
            if (!fields.TryGetProperty(member.Name, out memberValue))
            {
                string? formerName = member.FormerNames.FirstOrDefault(name => fields.TryGetProperty(name, out _));
                if (formerName is null || !fields.TryGetProperty(formerName, out memberValue))
                    continue;
            }
            member.SetValue(instance, ReadValue(memberValue, member.ValueType));
        }
    }

    private static void WriteValue(Utf8JsonWriter writer, object? value, Type declaredType, HashSet<object> visited)
    {
        Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
        if (value is null)
        {
            writer.WriteNullValue();
            return;
        }
        if (type == typeof(bool)) { writer.WriteBooleanValue((bool)value); return; }
        if (type == typeof(string) || type == typeof(char)) { writer.WriteStringValue(value.ToString()); return; }
        if (type.IsEnum)
        {
            Type underlyingType = Enum.GetUnderlyingType(type);
            if (underlyingType == typeof(byte) || underlyingType == typeof(ushort) || underlyingType == typeof(uint) ||
                underlyingType == typeof(ulong))
                writer.WriteNumberValue(Convert.ToUInt64(value));
            else
                writer.WriteNumberValue(Convert.ToInt64(value));
            return;
        }
        if (type == typeof(sbyte)) { writer.WriteNumberValue((sbyte)value); return; }
        if (type == typeof(short)) { writer.WriteNumberValue((short)value); return; }
        if (type == typeof(int)) { writer.WriteNumberValue((int)value); return; }
        if (type == typeof(long)) { writer.WriteNumberValue((long)value); return; }
        if (type == typeof(byte)) { writer.WriteNumberValue((byte)value); return; }
        if (type == typeof(ushort)) { writer.WriteNumberValue((ushort)value); return; }
        if (type == typeof(uint)) { writer.WriteNumberValue((uint)value); return; }
        if (type == typeof(ulong)) { writer.WriteNumberValue((ulong)value); return; }
        if (type == typeof(float)) { writer.WriteNumberValue((float)value); return; }
        if (type == typeof(double)) { writer.WriteNumberValue((double)value); return; }
        if (type == typeof(decimal)) { writer.WriteNumberValue((decimal)value); return; }
        if (type == typeof(UUID)) { writer.WriteStringValue(value.ToString()); return; }
        if (value is Entity entity) { writer.WriteStringValue(entity.uuid.ToString()); return; }
        if (value is Asset asset) { writer.WriteStringValue(asset.uuid.ToString()); return; }
        if (type == typeof(Vector2))
        {
            Vector2 vector = (Vector2)value;
            WriteNumbers(writer, vector.x, vector.y);
            return;
        }
        if (type == typeof(Vector3))
        {
            Vector3 vector = (Vector3)value;
            WriteNumbers(writer, vector.x, vector.y, vector.z);
            return;
        }
        if (type == typeof(Vector4))
        {
            Vector4 vector = (Vector4)value;
            WriteNumbers(writer, vector.x, vector.y, vector.z, vector.w);
            return;
        }
        if (type == typeof(Quaternion))
        {
            Quaternion quaternion = (Quaternion)value;
            WriteNumbers(writer, quaternion.x, quaternion.y, quaternion.z, quaternion.w);
            return;
        }
        if (type == typeof(Matrix4))
        {
            Matrix4 matrix = (Matrix4)value;
            writer.WriteStartArray();
            for (int index = 0; index < 16; ++index)
                writer.WriteNumberValue(matrix[index]);
            writer.WriteEndArray();
            return;
        }

        bool tracked = !type.IsValueType;
        if (tracked && !visited.Add(value))
            throw new InvalidOperationException($"Managed state contains a reference cycle at {type.FullName}.");
        try
        {
            if (type.IsArray)
            {
                Type elementType = type.GetElementType()!;
                writer.WriteStartArray();
                foreach (object? element in (Array)value)
                    WriteValue(writer, element, elementType, visited);
                writer.WriteEndArray();
                return;
            }
            if (IsGeneric(type, typeof(List<>)))
            {
                Type elementType = type.GetGenericArguments()[0];
                writer.WriteStartArray();
                foreach (object? element in (IEnumerable)value)
                    WriteValue(writer, element, elementType, visited);
                writer.WriteEndArray();
                return;
            }
            if (IsGeneric(type, typeof(Dictionary<,>)))
            {
                Type[] arguments = type.GetGenericArguments();
                writer.WriteStartArray();
                foreach (DictionaryEntry entry in (IDictionary)value)
                {
                    writer.WriteStartObject();
                    writer.WritePropertyName("Key");
                    WriteValue(writer, entry.Key, arguments[0], visited);
                    writer.WritePropertyName("Value");
                    WriteValue(writer, entry.Value, arguments[1], visited);
                    writer.WriteEndObject();
                }
                writer.WriteEndArray();
                return;
            }

            writer.WriteStartObject();
            foreach (ScriptMember member in ScriptMetadata.Discover(type, scriptType: false))
            {
                writer.WritePropertyName(member.Name);
                WriteValue(writer, member.GetValue(value), member.ValueType, visited);
            }
            writer.WriteEndObject();
        }
        finally
        {
            if (tracked)
                visited.Remove(value);
        }
    }

    private static object? ReadValue(JsonElement value, Type declaredType)
    {
        Type? nullable = Nullable.GetUnderlyingType(declaredType);
        Type type = nullable ?? declaredType;
        if (value.ValueKind == JsonValueKind.Null)
            return nullable is not null || !type.IsValueType ? null : Activator.CreateInstance(type);
        if (type == typeof(bool)) return value.GetBoolean();
        if (type == typeof(string)) return value.GetString();
        if (type == typeof(char)) return value.GetString() is { Length: > 0 } text ? text[0] : '\0';
        if (type.IsEnum)
        {
            Type underlyingType = Enum.GetUnderlyingType(type);
            return underlyingType == typeof(byte) || underlyingType == typeof(ushort) || underlyingType == typeof(uint) ||
                   underlyingType == typeof(ulong)
                     ? Enum.ToObject(type, value.GetUInt64())
                     : Enum.ToObject(type, value.GetInt64());
        }
        if (type == typeof(sbyte)) return value.GetSByte();
        if (type == typeof(short)) return value.GetInt16();
        if (type == typeof(int)) return value.GetInt32();
        if (type == typeof(long)) return value.GetInt64();
        if (type == typeof(byte)) return value.GetByte();
        if (type == typeof(ushort)) return value.GetUInt16();
        if (type == typeof(uint)) return value.GetUInt32();
        if (type == typeof(ulong)) return value.GetUInt64();
        if (type == typeof(float)) return value.GetSingle();
        if (type == typeof(double)) return value.GetDouble();
        if (type == typeof(decimal)) return value.GetDecimal();
        if (type == typeof(UUID)) return ParseUuid(value.GetString());
        if (typeof(Entity).IsAssignableFrom(type))
        {
            var entity = (Entity)(Activator.CreateInstance(type, nonPublic: true) ?? throw new InvalidOperationException($"Cannot create {type}."));
            entity.m_ManagedUuid = ParseUuid(value.GetString());
            return entity;
        }
        if (typeof(Asset).IsAssignableFrom(type))
        {
            var asset = (Asset)(Activator.CreateInstance(type, nonPublic: true) ?? throw new InvalidOperationException($"Cannot create {type}."));
            asset.m_ManagedUuid = ParseUuid(value.GetString());
            return asset;
        }
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
        if (type == typeof(Quaternion))
        {
            float[] elements = ReadNumbers(value, 4);
            return new Quaternion(elements[0], elements[1], elements[2], elements[3]);
        }
        if (type == typeof(Matrix4))
        {
            float[] elements = ReadNumbers(value, 16);
            var matrix = new Matrix4();
            for (int index = 0; index < 16; ++index)
                matrix[index] = elements[index];
            return matrix;
        }
        if (type.IsArray)
        {
            Type elementType = type.GetElementType()!;
            Array array = Array.CreateInstance(elementType, value.GetArrayLength());
            int index = 0;
            foreach (JsonElement element in value.EnumerateArray())
                array.SetValue(ReadValue(element, elementType), index++);
            return array;
        }
        if (IsGeneric(type, typeof(List<>)))
        {
            Type elementType = type.GetGenericArguments()[0];
            var list = (IList)(Activator.CreateInstance(type) ?? throw new InvalidOperationException($"Cannot create {type}."));
            foreach (JsonElement element in value.EnumerateArray())
                list.Add(ReadValue(element, elementType));
            return list;
        }
        if (IsGeneric(type, typeof(Dictionary<,>)))
        {
            Type[] arguments = type.GetGenericArguments();
            var dictionary = (IDictionary)(Activator.CreateInstance(type) ?? throw new InvalidOperationException($"Cannot create {type}."));
            foreach (JsonElement entry in value.EnumerateArray())
                dictionary.Add(ReadValue(entry.GetProperty("Key"), arguments[0]), ReadValue(entry.GetProperty("Value"), arguments[1]));
            return dictionary;
        }

        object instance = Activator.CreateInstance(type, nonPublic: true) ?? throw new InvalidOperationException($"Cannot create {type}.");
        ScriptMember[] members = ScriptMetadata.Discover(type, scriptType: false);
        foreach (ScriptMember member in members)
        {
            if (member.IsReadOnly)
                continue;
            JsonElement memberValue;
            if (!value.TryGetProperty(member.Name, out memberValue))
            {
                string? formerName = member.FormerNames.FirstOrDefault(name => value.TryGetProperty(name, out _));
                if (formerName is null || !value.TryGetProperty(formerName, out memberValue))
                    continue;
            }
            member.SetValue(instance, ReadValue(memberValue, member.ValueType));
        }
        return instance;
    }

    private static void WriteNumbers(Utf8JsonWriter writer, params float[] values)
    {
        writer.WriteStartArray();
        foreach (float value in values)
            writer.WriteNumberValue(value);
        writer.WriteEndArray();
    }

    private static float[] ReadNumbers(JsonElement value, int count)
    {
        if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() < count)
            throw new JsonException($"Expected an array with at least {count} numeric values.");
        var result = new float[count];
        int index = 0;
        foreach (JsonElement element in value.EnumerateArray())
        {
            if (index == count)
                break;
            result[index++] = element.GetSingle();
        }
        return result;
    }

    private static bool IsGeneric(Type type, Type definition) => type.IsGenericType && type.GetGenericTypeDefinition() == definition;

    private static UUID ParseUuid(string? value)
    {
        string text = (value ?? string.Empty).Replace("-", string.Empty, StringComparison.Ordinal);
        if (text.Length != 32)
            throw new JsonException("Expected a 128-bit UUID string.");
        try
        {
            return new UUID(Convert.ToUInt32(text.Substring(0, 8), 16), Convert.ToUInt32(text.Substring(8, 8), 16),
                            Convert.ToUInt32(text.Substring(16, 8), 16), Convert.ToUInt32(text.Substring(24, 8), 16));
        }
        catch (FormatException error)
        {
            throw new JsonException("Expected a hexadecimal UUID string.", error);
        }
        catch (OverflowException error)
        {
            throw new JsonException("Expected a 128-bit UUID string.", error);
        }
    }

    private static string GetTypeName(Type type)
    {
        string fullName = type.FullName ?? type.Name;
        string typeNamespace = type.Namespace ?? string.Empty;
        return typeNamespace.Length == 0 ? fullName : fullName[(typeNamespace.Length + 1)..];
    }
}
