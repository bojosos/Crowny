using System.Reflection;

namespace Crowny.ManagedHost;

internal sealed class ScriptMember
{
    private readonly MemberInfo _member;

    internal ScriptMember(FieldInfo field)
    {
        _member = field;
        Name = field.Name;
        ValueType = field.FieldType;
        IsReadOnly = field.IsInitOnly;
        FormerNames = FormerNamesFor(field);
    }

    internal ScriptMember(PropertyInfo property)
    {
        _member = property;
        Name = property.Name;
        ValueType = property.PropertyType;
        IsReadOnly = property.SetMethod is null;
        FormerNames = FormerNamesFor(property);
    }

    internal string Name { get; }
    internal Type ValueType { get; }
    internal bool IsReadOnly { get; }
    internal string[] FormerNames { get; }

    internal object? GetValue(object instance) => _member switch
    {
        FieldInfo field => field.GetValue(instance),
        PropertyInfo property => property.GetValue(instance),
        _ => throw new InvalidOperationException($"Unsupported managed member {_member}.")
    };

    internal void SetValue(object instance, object? value)
    {
        if (IsReadOnly)
            return;
        switch (_member)
        {
        case FieldInfo field:
            field.SetValue(instance, value);
            break;
        case PropertyInfo property:
            property.SetValue(instance, value);
            break;
        default:
            throw new InvalidOperationException($"Unsupported managed member {_member}.");
        }
    }

    private static string[] FormerNamesFor(MemberInfo member) =>
        member.GetCustomAttributes<FormerlySerializedAs>().Select(attribute => attribute.OldName).ToArray();
}

internal static class ScriptMetadata
{
    internal static ScriptMember[] Discover(Type type, bool scriptType)
    {
        var hierarchy = new Stack<Type>();
        Type stopType = scriptType ? typeof(EntityBehaviour) : typeof(object);
        for (Type? current = type; current is not null && current != stopType && current != typeof(object); current = current.BaseType)
            hierarchy.Push(current);

        var members = new List<(int TypeOrder, int KindOrder, int MetadataToken, ScriptMember Member)>();
        int typeOrder = 0;
        while (hierarchy.Count != 0)
        {
            Type current = hierarchy.Pop();
            foreach (FieldInfo field in current.GetFields(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public |
                                                          BindingFlags.NonPublic))
            {
                if (field.IsStatic || !CanSerialize(field.FieldType) || field.GetCustomAttribute<DontSerializeField>() is not null ||
                    (!field.IsPublic && field.GetCustomAttribute<SerializeField>() is null))
                    continue;
                members.Add((typeOrder, 0, field.MetadataToken, new ScriptMember(field)));
            }
            foreach (PropertyInfo property in current.GetProperties(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public |
                                                                     BindingFlags.NonPublic))
            {
                MethodInfo? getter = property.GetMethod;
                MethodInfo? setter = property.SetMethod;
                bool hasPublicAccessor = getter?.IsPublic == true || setter?.IsPublic == true;
                if (getter is null || getter.IsStatic || setter?.IsStatic == true || !CanSerialize(property.PropertyType) ||
                    property.GetIndexParameters().Length != 0 ||
                    property.GetCustomAttribute<DontSerializeField>() is not null ||
                    (!hasPublicAccessor && property.GetCustomAttribute<SerializeField>() is null))
                    continue;
                members.Add((typeOrder, 1, property.MetadataToken, new ScriptMember(property)));
            }
            ++typeOrder;
        }

        ScriptMember[] result = members.OrderBy(member => member.TypeOrder)
                                       .ThenBy(member => member.KindOrder)
                                       .ThenBy(member => member.MetadataToken)
                                       .Select(member => member.Member)
                                       .ToArray();
        var names = new HashSet<string>(StringComparer.Ordinal);
        foreach (ScriptMember member in result)
        {
            if (!names.Add(member.Name))
                throw new InvalidOperationException($"Managed type {type.FullName} has more than one serializable member named {member.Name}.");
        }
        return result;
    }

    private static bool CanSerialize(Type declaredType)
    {
        Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
        if (type.IsEnum || type == typeof(bool) || type == typeof(char) || type == typeof(sbyte) || type == typeof(byte) ||
            type == typeof(short) || type == typeof(ushort) || type == typeof(int) || type == typeof(uint) || type == typeof(long) ||
            type == typeof(ulong) || type == typeof(float) || type == typeof(double) || type == typeof(string) || type == typeof(UUID) ||
            type == typeof(Vector2) || type == typeof(Vector3) || type == typeof(Vector4) || type == typeof(Quaternion) ||
            type == typeof(Matrix4) || typeof(Entity).IsAssignableFrom(type) || typeof(Asset).IsAssignableFrom(type))
            return true;
        if (type.IsArray)
            return CanSerialize(type.GetElementType()!);
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>))
            return CanSerialize(type.GetGenericArguments()[0]);
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Dictionary<,>))
        {
            Type[] arguments = type.GetGenericArguments();
            return CanSerialize(arguments[0]) && CanSerialize(arguments[1]);
        }
        return type.GetCustomAttribute<SerializeObject>() is not null || typeof(Component).IsAssignableFrom(type);
    }
}
