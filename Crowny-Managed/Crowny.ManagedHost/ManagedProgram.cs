using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using Crowny.ManagedHost.Interop;

namespace Crowny.ManagedHost;

internal sealed class ManagedProgram
{
    private sealed record ScriptRecord(object Instance, Type Type, Entity Entity, Dictionary<NativeEventKind, Delegate> Callbacks);
    private sealed record CatalogTypeIdentity(string Assembly, string Namespace, string TypeName);

    private readonly Dictionary<string, Type> _types = new(StringComparer.Ordinal);
    private readonly Dictionary<Type, ScriptMember[]> _members = new();
    private readonly Dictionary<Type, Dictionary<NativeEventKind, MethodInfo?>> _callbacks = new();
    private readonly Dictionary<ulong, ScriptRecord> _instances = new();
    private GameLoadContext? _loadContext;
    private WeakReference? _unloadReference;
    private ulong _nextHandle = 1;
    private ulong _generation;

    internal ulong Generation => _generation;

    internal void Load(string assemblyPath, ulong generation)
    {
        if (_loadContext is not null)
            throw new InvalidOperationException("A managed game program is already loaded.");
        string fullPath = Path.GetFullPath(assemblyPath);
        var context = new GameLoadContext(fullPath);
        var discoveredTypes = new Dictionary<string, Type>(StringComparer.Ordinal);
        var discoveredMembers = new Dictionary<Type, ScriptMember[]>();
        var discoveredCallbacks = new Dictionary<Type, Dictionary<NativeEventKind, MethodInfo?>>();
        try
        {
            Assembly assembly = context.LoadFromAssemblyPath(fullPath);
            foreach (Type type in assembly.GetTypes().Where(type => !type.IsAbstract && typeof(EntityBehaviour).IsAssignableFrom(type)))
            {
                discoveredTypes.Add(Identity(type), type);
                discoveredMembers.Add(type, ScriptMetadata.Discover(type, scriptType: true));
                discoveredCallbacks.Add(type, DiscoverCallbacks(type));
            }
        }
        catch (ReflectionTypeLoadException error)
        {
            context.Unload();
            string details = string.Join(Environment.NewLine,
                                         error.LoaderExceptions.Where(exception => exception is not null)
                                                               .Select(exception => exception!.ToString()));
            throw new InvalidOperationException($"Managed script type discovery failed.{Environment.NewLine}{details}", error);
        }
        catch
        {
            context.Unload();
            throw;
        }

        foreach ((string identity, Type type) in discoveredTypes)
            _types.Add(identity, type);
        foreach ((Type type, ScriptMember[] members) in discoveredMembers)
            _members.Add(type, members);
        foreach ((Type type, Dictionary<NativeEventKind, MethodInfo?> callbacks) in discoveredCallbacks)
            _callbacks.Add(type, callbacks);
        _loadContext = context;
        _generation = generation;
    }

    internal bool Unload()
    {
        _instances.Clear();
        _types.Clear();
        _members.Clear();
        _callbacks.Clear();
        _generation = 0;
        _nextHandle = 1;
        _unloadReference = BeginUnload();
        if (_unloadReference is null)
            return true;
        for (int pass = 0; pass < 8 && _unloadReference.IsAlive; ++pass)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
        return !_unloadReference.IsAlive;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private WeakReference? BeginUnload()
    {
        GameLoadContext? context = _loadContext;
        _loadContext = null;
        if (context is null)
            return null;
        var reference = new WeakReference(context, trackResurrection: true);
        context.Unload();
        return reference;
    }

    internal string GetCatalogJson()
    {
        Type[] orderedTypes = _types.Values.OrderBy(type => type.FullName, StringComparer.Ordinal).ToArray();
        Dictionary<string, int> legacyIdentityCounts = orderedTypes.Where(type => type.IsNested)
                                                                    .GroupBy(LegacyIdentity, StringComparer.Ordinal)
                                                                    .ToDictionary(group => group.Key, group => group.Count(),
                                                                                  StringComparer.Ordinal);
        var types = orderedTypes.Select(type => new
        {
            StableId = StableId(type.Assembly.GetName().Name + ":" + type.FullName),
            Assembly = type.Assembly.GetName().Name,
            Namespace = type.Namespace ?? string.Empty,
            TypeName = TypeName(type),
            FormerIdentities = FormerIdentities(type, legacyIdentityCounts),
            BaseType = type.BaseType is { } baseType && baseType != typeof(EntityBehaviour) ? CatalogIdentity(baseType) : null,
            Events = LifecycleEvents(type),
            Fields = Members(type).Select(member => new
            {
                StableId = StableId(type.Assembly.GetName().Name + ":" + type.FullName + ":" + member.Name),
                member.Name,
                member.FormerNames,
                ValueKind = ValueKind(member.ValueType),
                ElementKind = ElementKind(member.ValueType),
                KeyKind = KeyKind(member.ValueType),
                DeclaredType = DeclaredType(member.ValueType),
                IsNullable = Nullable.GetUnderlyingType(member.ValueType) is not null || !member.ValueType.IsValueType,
                member.IsReadOnly
            })
        });
        return JsonSerializer.Serialize(new { ManifestVersion = 1, Types = types });
    }

    internal ulong Create(string assemblyName, string typeNamespace, string typeName, Guid entity, ReadOnlySpan<byte> state)
    {
        string identity = assemblyName + ":" + (string.IsNullOrEmpty(typeNamespace) ? typeName : typeNamespace + "." + typeName);
        if (!_types.TryGetValue(identity, out Type? type))
            throw new TypeLoadException(identity);
        object instance = Activator.CreateInstance(type, nonPublic: true) ?? throw new InvalidOperationException($"Cannot create {identity}.");
        var managedEntity = new Entity { m_ManagedUuid = ToCrownyUuid(entity) };
        ((Component)instance).m_ManagedEntity = managedEntity;
        if (!state.IsEmpty)
            ManagedStateCodec.Apply(instance, Members(type), state);
        if (_nextHandle == 0)
            throw new InvalidOperationException("Managed script handles are exhausted.");
        ulong handle = _nextHandle++;
        _instances.Add(handle, new ScriptRecord(instance, type, managedEntity, BindCallbacks(type, instance)));
        return handle;
    }

    internal void Destroy(ulong handle)
    {
        if (!_instances.Remove(handle))
            throw new KeyNotFoundException($"Unknown script handle {handle}.");
    }

    internal void Dispatch(ulong handle, NativeEventKind kind, float deltaTime, Guid otherEntityId,
                           ReadOnlySpan<NativeContactPoint> contacts)
    {
        ScriptRecord record = Get(handle);
        if (!record.Callbacks.TryGetValue(kind, out Delegate? callback))
            return;
        using ManagedRuntimeContext.CallbackScope callbackScope = ManagedRuntimeContext.Push(deltaTime);
        switch (callback)
        {
        case Action lifecycle:
            lifecycle();
            break;
        case Action<Entity> trigger:
            trigger(CreateEntity(otherEntityId));
            break;
        case Action<Collision2D> collision2D:
            collision2D(CreateCollision2D(CreateEntity(otherEntityId), contacts));
            break;
        case Action<Collision3D> collision3D:
            collision3D(CreateCollision3D(CreateEntity(otherEntityId), contacts));
            break;
        default:
            throw new InvalidOperationException($"Unsupported managed callback delegate {callback.GetType()}.");
        }
    }

    internal byte[] CaptureState(ulong handle)
    {
        ScriptRecord record = Get(handle);
        return ManagedStateCodec.Capture(record.Type, record.Instance, Members(record.Type));
    }

    internal void ApplyState(ulong handle, ReadOnlySpan<byte> state)
    {
        ScriptRecord record = Get(handle);
        ScriptMember[] members = Members(record.Type);
        byte[] previousState = ManagedStateCodec.Capture(record.Type, record.Instance, members);
        try
        {
            ManagedStateCodec.Apply(record.Instance, members, state);
        }
        catch (Exception applyError)
        {
            try
            {
                ManagedStateCodec.Apply(record.Instance, members, previousState);
            }
            catch (Exception rollbackError)
            {
                throw new AggregateException("Managed state application and rollback both failed.", applyError, rollbackError);
            }
            throw;
        }
    }

    private ScriptRecord Get(ulong handle) =>
        _instances.TryGetValue(handle, out ScriptRecord? record) ? record : throw new KeyNotFoundException($"Unknown script handle {handle}.");

    private ScriptMember[] Members(Type type) =>
        _members.TryGetValue(type, out ScriptMember[]? members)
          ? members
          : throw new InvalidOperationException($"Managed metadata for {type.FullName} is unavailable.");

    private Dictionary<NativeEventKind, MethodInfo?> Callbacks(Type type) =>
        _callbacks.TryGetValue(type, out Dictionary<NativeEventKind, MethodInfo?>? callbacks)
          ? callbacks
          : throw new InvalidOperationException($"Managed callbacks for {type.FullName} are unavailable.");

    private Dictionary<NativeEventKind, Delegate> BindCallbacks(Type type, object instance)
    {
        var bound = new Dictionary<NativeEventKind, Delegate>();
        foreach ((NativeEventKind kind, MethodInfo? method) in Callbacks(type))
        {
            if (method is null)
                continue;
            Type? parameterType = CallbackSignature(kind).ParameterType;
            Type delegateType = parameterType is null ? typeof(Action) : typeof(Action<>).MakeGenericType(parameterType);
            bound.Add(kind, method.CreateDelegate(delegateType, instance));
        }
        return bound;
    }

    private static string Identity(Type type) => type.Assembly.GetName().Name + ":" + type.FullName;

    private static CatalogTypeIdentity CatalogIdentity(Type type) =>
        new(type.Assembly.GetName().Name ?? string.Empty, type.Namespace ?? string.Empty, TypeName(type));

    private static string LegacyIdentity(Type type) =>
        (type.Assembly.GetName().Name ?? string.Empty) + ":" + (type.Namespace ?? string.Empty) + ":" + type.Name;

    private static CatalogTypeIdentity[] FormerIdentities(Type type, IReadOnlyDictionary<string, int> legacyIdentityCounts)
    {
        if (!type.IsNested || legacyIdentityCounts[LegacyIdentity(type)] != 1)
            return [];
        return [new CatalogTypeIdentity(type.Assembly.GetName().Name ?? string.Empty, type.Namespace ?? string.Empty, type.Name)];
    }

    private static string TypeName(Type type)
    {
        string fullName = type.FullName ?? type.Name;
        string typeNamespace = type.Namespace ?? string.Empty;
        return typeNamespace.Length == 0 ? fullName : fullName[(typeNamespace.Length + 1)..];
    }

    private static UUID ToCrownyUuid(Guid value)
    {
        string text = value.ToString("N");
        return new UUID(Convert.ToUInt32(text.Substring(0, 8), 16), Convert.ToUInt32(text.Substring(8, 8), 16),
                        Convert.ToUInt32(text.Substring(16, 8), 16), Convert.ToUInt32(text.Substring(24, 8), 16));
    }

    private static Entity CreateEntity(Guid value) => new() { m_ManagedUuid = ToCrownyUuid(value) };

    private string[] LifecycleEvents(Type type)
    {
        return Callbacks(type).Where(callback => callback.Value is not null)
                   .OrderBy(callback => callback.Key)
                   .Select(callback => callback.Key)
                   .Select(kind => kind.ToString())
                   .ToArray();
    }

    private static Dictionary<NativeEventKind, MethodInfo?> DiscoverCallbacks(Type type)
    {
        var callbacks = new Dictionary<NativeEventKind, MethodInfo?>();
        foreach (NativeEventKind kind in Enum.GetValues<NativeEventKind>())
        {
            (string methodName, Type? parameterType) = CallbackSignature(kind);
            callbacks.Add(kind, FindCallback(type, methodName, parameterType));
        }
        return callbacks;
    }

    private static (string MethodName, Type? ParameterType) CallbackSignature(NativeEventKind kind) => kind switch
    {
        NativeEventKind.Start => ("Start", null),
        NativeEventKind.Update => ("Update", null),
        NativeEventKind.Destroy => ("OnDestroy", null),
        NativeEventKind.CollisionEnter2D => ("OnCollisionEnter2D", typeof(Collision2D)),
        NativeEventKind.CollisionStay2D => ("OnCollisionStay2D", typeof(Collision2D)),
        NativeEventKind.CollisionExit2D => ("OnCollisionExit2D", typeof(Collision2D)),
        NativeEventKind.TriggerEnter2D => ("OnTriggerEnter2D", typeof(Entity)),
        NativeEventKind.TriggerStay2D => ("OnTriggerStay2D", typeof(Entity)),
        NativeEventKind.TriggerExit2D => ("OnTriggerExit2D", typeof(Entity)),
        NativeEventKind.CollisionEnter3D => ("OnCollisionEnter3D", typeof(Collision3D)),
        NativeEventKind.CollisionStay3D => ("OnCollisionStay3D", typeof(Collision3D)),
        NativeEventKind.CollisionExit3D => ("OnCollisionExit3D", typeof(Collision3D)),
        NativeEventKind.TriggerEnter3D => ("OnTriggerEnter3D", typeof(Entity)),
        NativeEventKind.TriggerStay3D => ("OnTriggerStay3D", typeof(Entity)),
        NativeEventKind.TriggerExit3D => ("OnTriggerExit3D", typeof(Entity)),
        _ => throw new ArgumentOutOfRangeException(nameof(kind), kind, "Unknown managed event kind.")
    };

    private static MethodInfo? FindCallback(Type type, string methodName, Type? parameterType)
    {
        for (Type? current = type; current is not null && current != typeof(EntityBehaviour); current = current.BaseType)
        {
            foreach (MethodInfo method in current.GetMethods(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public |
                                                              BindingFlags.NonPublic))
            {
                if (method.Name != methodName || method.ContainsGenericParameters)
                    continue;
                ParameterInfo[] parameters = method.GetParameters();
                if (parameterType is null ? parameters.Length != 0 : parameters.Length != 1 || parameters[0].ParameterType != parameterType)
                    continue;
                if (method.ReturnType != typeof(void))
                    throw new InvalidOperationException($"Managed callback {current.FullName}.{methodName} must return void.");
                return method;
            }
        }
        return null;
    }

    private static Collision2D CreateCollision2D(Entity otherEntity, ReadOnlySpan<NativeContactPoint> contacts)
    {
        var points = new Vector2[contacts.Length];
        for (int index = 0; index < contacts.Length; ++index)
            points[index] = new Vector2(contacts[index].PositionX, contacts[index].PositionY);
        return new Collision2D { Colliders = [otherEntity], Points = points };
    }

    private static Collision3D CreateCollision3D(Entity otherEntity, ReadOnlySpan<NativeContactPoint> contacts)
    {
        var converted = new ContactPoint3D[contacts.Length];
        for (int index = 0; index < contacts.Length; ++index)
        {
            NativeContactPoint contact = contacts[index];
            converted[index] = new ContactPoint3D(new Vector3(contact.PositionX, contact.PositionY, contact.PositionZ),
                                                  new Vector3(contact.NormalX, contact.NormalY, contact.NormalZ), contact.Separation,
                                                  contact.Impulse);
        }
        return new Collision3D { Colliders = [otherEntity], Contacts = converted };
    }

    private static ulong StableId(string? text)
    {
        const ulong offset = 14695981039346656037;
        const ulong prime = 1099511628211;
        ulong hash = offset;
        foreach (byte value in System.Text.Encoding.UTF8.GetBytes(text ?? string.Empty))
            hash = (hash ^ value) * prime;
        return hash == 0 ? 1 : hash;
    }

    private static string ValueKind(Type type)
    {
        type = Nullable.GetUnderlyingType(type) ?? type;
        if (type == typeof(bool)) return "Boolean";
        if (type == typeof(string) || type == typeof(char)) return "String";
        if (type.IsEnum) return "Enum";
        if (type == typeof(float) || type == typeof(double) || type == typeof(decimal)) return "Float";
        if (type == typeof(byte) || type == typeof(ushort) || type == typeof(uint) || type == typeof(ulong)) return "UnsignedInteger";
        if (type == typeof(sbyte) || type == typeof(short) || type == typeof(int) || type == typeof(long)) return "SignedInteger";
        if (type == typeof(UUID)) return "Uuid";
        if (type == typeof(Vector2)) return "Vector2";
        if (type == typeof(Vector3)) return "Vector3";
        if (type == typeof(Vector4)) return "Vector4";
        if (type == typeof(Quaternion)) return "Quaternion";
        if (type == typeof(Matrix4)) return "Matrix4";
        if (typeof(Entity).IsAssignableFrom(type)) return "Entity";
        if (typeof(Asset).IsAssignableFrom(type)) return "Asset";
        if (type.IsArray) return "Array";
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>)) return "List";
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Dictionary<,>)) return "Dictionary";
        return "Object";
    }

    private static string? ElementKind(Type type)
    {
        if (type.IsArray)
            return ValueKind(type.GetElementType()!);
        if (type.IsGenericType && (type.GetGenericTypeDefinition() == typeof(List<>) ||
                                   type.GetGenericTypeDefinition() == typeof(Dictionary<,>)))
            return ValueKind(type.GetGenericArguments()[^1]);
        return null;
    }

    private static string? KeyKind(Type type) =>
        type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Dictionary<,>) ? ValueKind(type.GetGenericArguments()[0]) : null;

    private static CatalogTypeIdentity? DeclaredType(Type type)
    {
        type = Nullable.GetUnderlyingType(type) ?? type;
        return ValueKind(type) == "Object" ? CatalogIdentity(type) : null;
    }
}
