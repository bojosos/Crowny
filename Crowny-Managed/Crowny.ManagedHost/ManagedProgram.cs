using System.Reflection;
using System.Runtime.CompilerServices;
using Crowny.ManagedHost.Interop;

namespace Crowny.ManagedHost;

internal sealed class ManagedProgram
{
    private sealed record ScriptRecord(object Instance, Type Type, UUID Entity, Dictionary<NativeEventKind, Delegate> Callbacks);

    private readonly Dictionary<string, Type> _types = new(StringComparer.Ordinal);
    private readonly Dictionary<Type, ScriptMember[]> _members = new();
    private readonly Dictionary<Type, Dictionary<NativeEventKind, MethodInfo?>> _callbacks = new();
    private readonly Dictionary<ulong, ScriptRecord> _instances = new();
    private GameLoadContext? _loadContext;
    private WeakReference? _unloadReference;
    private ulong _nextHandle = 1;
    private ulong _generation;

    internal ulong Generation => _generation;

    internal Component? ResolveScriptComponent(UUID entity, Type requestedType)
    {
        foreach (ScriptRecord record in _instances.Values)
        {
            if (record.Entity == entity && requestedType.IsAssignableFrom(record.Type))
                return (Component)record.Instance;
        }
        return null;
    }

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
        return ManagedScriptCatalog.Capture(_types.Values);
    }

    internal ulong Create(string assemblyName, string typeNamespace, string typeName, Guid entity, ReadOnlySpan<byte> state)
    {
        string identity = assemblyName + ":" + (string.IsNullOrEmpty(typeNamespace) ? typeName : typeNamespace + "." + typeName);
        if (!_types.TryGetValue(identity, out Type? type))
            throw new TypeLoadException(identity);
        object instance = Activator.CreateInstance(type, nonPublic: true) ?? throw new InvalidOperationException($"Cannot create {identity}.");
        UUID managedEntity = ManagedRuntimeContext.FromGuid(entity);
        ((Component)instance).m_ManagedEntityId = managedEntity;
        string? preparationError = ManagedScriptLifecycle.TryPrepare(instance);
        if (preparationError is not null)
            throw new InvalidOperationException(preparationError);
        if (!state.IsEmpty)
            ManagedStateCodec.Apply(instance, Members(type), System.Text.Encoding.UTF8.GetString(state));
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
            collision2D(CreateCollision2D(CreateEntity(record.Entity), CreateEntity(otherEntityId), contacts));
            break;
        case Action<Collision3D> collision3D:
            collision3D(CreateCollision3D(CreateEntity(record.Entity), CreateEntity(otherEntityId), contacts));
            break;
        default:
            throw new InvalidOperationException($"Unsupported managed callback delegate {callback.GetType()}.");
        }
    }

    internal byte[] CaptureState(ulong handle)
    {
        ScriptRecord record = Get(handle);
        return System.Text.Encoding.UTF8.GetBytes(ManagedStateCodec.Capture(record.Type, record.Instance, Members(record.Type)));
    }

    internal void ApplyState(ulong handle, ReadOnlySpan<byte> state)
    {
        ScriptRecord record = Get(handle);
        string error = ManagedStateCodec.TryApply(record.Instance, System.Text.Encoding.UTF8.GetString(state));
        if (error is not null)
            throw new InvalidOperationException(error);
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
            ParameterInfo[] parameters = method.GetParameters();
            Type? parameterType = parameters.Length == 0 ? null : parameters[0].ParameterType;
            Type delegateType = parameterType is null ? typeof(Action) : typeof(Action<>).MakeGenericType(parameterType);
            bound.Add(kind, method.CreateDelegate(delegateType, instance));
        }
        return bound;
    }

    private static string Identity(Type type) => type.Assembly.GetName().Name + ":" + type.FullName;

    private static Entity CreateEntity(Guid value) => CreateEntity(ManagedRuntimeContext.FromGuid(value));

    private static Entity CreateEntity(UUID value) => new() { m_ManagedUuid = value };

    private static Dictionary<NativeEventKind, MethodInfo?> DiscoverCallbacks(Type type)
    {
        var callbacks = new Dictionary<NativeEventKind, MethodInfo?>();
        Dictionary<string, MethodInfo> discovered = ScriptCallbacks.Discover(type);
        foreach (NativeEventKind kind in Enum.GetValues<NativeEventKind>())
        {
            discovered.TryGetValue(kind.ToString(), out MethodInfo? callback);
            callbacks.Add(kind, callback);
        }
        return callbacks;
    }

    private static Collision2D CreateCollision2D(Entity selfEntity, Entity otherEntity, ReadOnlySpan<NativeContactPoint> contacts)
    {
        var points = new Vector2[contacts.Length];
        for (int index = 0; index < contacts.Length; ++index)
            points[index] = new Vector2(contacts[index].PositionX, contacts[index].PositionY);
        return new Collision2D { Colliders = [selfEntity, otherEntity], Points = points };
    }

    private static Collision3D CreateCollision3D(Entity selfEntity, Entity otherEntity, ReadOnlySpan<NativeContactPoint> contacts)
    {
        var converted = new ContactPoint3D[contacts.Length];
        for (int index = 0; index < contacts.Length; ++index)
        {
            NativeContactPoint contact = contacts[index];
            converted[index] = new ContactPoint3D(new Vector3(contact.PositionX, contact.PositionY, contact.PositionZ),
                                                  new Vector3(contact.NormalX, contact.NormalY, contact.NormalZ), contact.Separation,
                                                  contact.Impulse);
        }
        return new Collision3D { Colliders = [selfEntity, otherEntity], Contacts = converted };
    }

}
