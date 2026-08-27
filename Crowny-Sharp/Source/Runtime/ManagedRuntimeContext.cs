using System;
using System.Text;

namespace Crowny
{
    internal static unsafe class ManagedRuntimeContext
    {
        [ThreadStatic]
        private static float callbackDeltaTime;
        private static Action<int, string> logHandler;
        private static Func<UUID, string> getEntityName;
        private static Action<UUID, string> setEntityName;
        private static Func<string, UUID> findEntityByName;
        private static Func<UUID, UUID> getEntityParent;
        private static Action<UUID, UUID> setEntityParent;
        private static Action<UUID> destroyEntity;
        private static Func<ManagedBindingId, UUID, byte[], byte[]> hostBindingHandler;
        private static Func<UUID, Type, Component> scriptResolver;
#if !CROWNY_MONO
        private static ManagedNativeHostApi nativeHostApi;
#endif

        internal static float DeltaTime => callbackDeltaTime;

        internal static CallbackScope Push(float deltaTime)
        {
            float previous = callbackDeltaTime;
            callbackDeltaTime = deltaTime;
            return new CallbackScope(previous);
        }

        internal static void SetLogHandler(Action<int, string> handler)
        {
            logHandler = handler;
        }

        internal static void WriteLog(int severity, string message)
        {
            logHandler?.Invoke(severity, message);
        }

        internal static void SetEntityHandlers(Func<UUID, string> getName, Action<UUID, string> setName, Func<string, UUID> findByName,
                                               Func<UUID, UUID> getParent, Action<UUID, UUID> setParent, Action<UUID> destroy)
        {
            getEntityName = getName;
            setEntityName = setName;
            findEntityByName = findByName;
            getEntityParent = getParent;
            setEntityParent = setParent;
            destroyEntity = destroy;
        }

        internal static void SetHostBindingHandler(Func<ManagedBindingId, UUID, byte[], byte[]> handler)
        {
            hostBindingHandler = handler;
        }

#if !CROWNY_MONO
        internal static void SetNativeHostApi(ManagedNativeHostApi api)
        {
            nativeHostApi = api;
        }
#endif

        internal static void SetScriptResolver(Func<UUID, Type, Component> resolver)
        {
            scriptResolver = resolver;
        }

        internal static string GetEntityName(UUID entity) =>
            getEntityName != null ? getEntityName(entity) : throw new InvalidOperationException("Managed entity bindings are unavailable.");

        internal static void SetEntityName(UUID entity, string name)
        {
            if (setEntityName == null)
                throw new InvalidOperationException("Managed entity bindings are unavailable.");
            setEntityName(entity, name);
        }

        internal static UUID FindEntityByName(string name) =>
            findEntityByName != null ? findEntityByName(name) : throw new InvalidOperationException("Managed entity bindings are unavailable.");

        internal static UUID GetEntityParent(UUID entity) =>
            getEntityParent != null ? getEntityParent(entity) : throw new InvalidOperationException("Managed entity bindings are unavailable.");

        internal static void SetEntityParent(UUID entity, UUID parent)
        {
            if (setEntityParent == null)
                throw new InvalidOperationException("Managed entity bindings are unavailable.");
            setEntityParent(entity, parent);
        }

        internal static void DestroyEntity(UUID entity)
        {
            if (destroyEntity == null)
                throw new InvalidOperationException("Managed entity bindings are unavailable.");
            destroyEntity(entity);
        }

        internal static T GetComponent<T>(UUID entity) where T : Component
        {
            Type type = typeof(T);
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
                return scriptResolver?.Invoke(entity, type) as T;
            if (!HasComponent<T>(entity))
                return null;
            return CreateComponent<T>(entity);
        }

        internal static bool HasComponent<T>(UUID entity) where T : Component
        {
            Type type = typeof(T);
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
                return scriptResolver?.Invoke(entity, type) != null;
            return ReadBoolean(Invoke(ManagedBindingId.EntityHasComponent, entity, EncodeType(type)));
        }

        internal static T AddComponent<T>(UUID entity) where T : Component
        {
            Type type = typeof(T);
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
                throw new InvalidOperationException("Managed script components must be attached through scene script metadata.");
            Invoke(ManagedBindingId.EntityAddComponent, entity, EncodeType(type));
            return CreateComponent<T>(entity);
        }

        internal static void RemoveComponent<T>(UUID entity) where T : Component
        {
            Type type = typeof(T);
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
                throw new InvalidOperationException("Managed script components must be removed through scene script metadata.");
            Invoke(ManagedBindingId.EntityRemoveComponent, entity, EncodeType(type));
        }

        internal static Vector3 GetVector3(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, 12);
            return new Vector3(ReadSingle(value, 0), ReadSingle(value, 4), ReadSingle(value, 8));
        }

        internal static void SetVector3(ManagedBindingId binding, UUID entity, Vector3 value)
        {
            Invoke(binding, entity, Encode(value.x, value.y, value.z));
        }

        internal static Quaternion GetQuaternion(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, 16);
            return new Quaternion(ReadSingle(value, 0), ReadSingle(value, 4), ReadSingle(value, 8), ReadSingle(value, 12));
        }

        internal static void SetQuaternion(ManagedBindingId binding, UUID entity, Quaternion value)
        {
            Invoke(binding, entity, Encode(value.x, value.y, value.z, value.w));
        }

        internal static bool GetBindingBoolean(ManagedBindingId binding, UUID entity) =>
            ReadBoolean(Invoke(binding, entity, Array.Empty<byte>()));

        internal static void SetBindingBoolean(ManagedBindingId binding, UUID entity, bool value)
        {
            Invoke(binding, entity, new[] { value ? (byte)1 : (byte)0 });
        }

        internal static int GetBindingInt32(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, sizeof(int));
            return BitConverter.ToInt32(value, 0);
        }

        internal static uint GetBindingUInt32(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, sizeof(uint));
            return BitConverter.ToUInt32(value, 0);
        }

        internal static void SetBindingInt32(ManagedBindingId binding, UUID entity, int value)
        {
            Invoke(binding, entity, BitConverter.GetBytes(value));
        }

        internal static void SetBindingUInt32(ManagedBindingId binding, UUID entity, uint value)
        {
            Invoke(binding, entity, BitConverter.GetBytes(value));
        }

        internal static float GetBindingFloat(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, sizeof(float));
            return ReadSingle(value, 0);
        }

        internal static void SetBindingFloat(ManagedBindingId binding, UUID entity, float value)
        {
            Invoke(binding, entity, BitConverter.GetBytes(value));
        }

        internal static Vector2 GetBindingVector2(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, 2 * sizeof(float));
            return new Vector2(ReadSingle(value, 0), ReadSingle(value, sizeof(float)));
        }

        internal static void SetBindingVector2(ManagedBindingId binding, UUID entity, Vector2 value)
        {
            Invoke(binding, entity, Encode(value.x, value.y));
        }

        internal static void InvokeBinding(ManagedBindingId binding, UUID entity)
        {
            Invoke(binding, entity, Array.Empty<byte>());
        }

        internal static void InvokeBinding(ManagedBindingId binding, UUID entity, Vector2 value, int mode)
        {
            Invoke(binding, entity, Encode(value, mode));
        }

        internal static void InvokeBinding(ManagedBindingId binding, UUID entity, Vector2 first, Vector2 second, int mode)
        {
            Invoke(binding, entity, Encode(first, second, mode));
        }

        internal static void InvokeBinding(ManagedBindingId binding, UUID entity, float value, int mode)
        {
            byte[] result = new byte[2 * sizeof(int)];
            Buffer.BlockCopy(BitConverter.GetBytes(value), 0, result, 0, sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(mode), 0, result, sizeof(float), sizeof(int));
            Invoke(binding, entity, result);
        }

        internal static UUID GetBindingUuid(ManagedBindingId binding, UUID entity)
        {
            byte[] value = Invoke(binding, entity, Array.Empty<byte>());
            RequireLength(value, 16);
            return DecodeUuid(value);
        }

        internal static void SetBindingUuid(ManagedBindingId binding, UUID entity, UUID value)
        {
            Invoke(binding, entity, Encode(value));
        }

        internal static T CreateAsset<T>(UUID uuid) where T : Asset
        {
            if (uuid == UUID.Empty)
                return null;
            T asset = Activator.CreateInstance<T>();
            asset.m_ManagedUuid = uuid;
            return asset;
        }

        internal static Matrix4 GetMatrix4(ManagedBindingId binding, UUID entity)
        {
            return DecodeMatrix(Invoke(binding, entity, Array.Empty<byte>()));
        }

        internal static float GetBindingFloat(ManagedBindingId binding, Matrix4 matrix)
        {
            byte[] value = Invoke(binding, UUID.Empty, Encode(matrix));
            RequireLength(value, sizeof(float));
            return ReadSingle(value, 0);
        }

        internal static Matrix4 GetBindingMatrix4(ManagedBindingId binding, Matrix4 matrix)
        {
            return DecodeMatrix(Invoke(binding, UUID.Empty, Encode(matrix)));
        }

        internal static Matrix4 GetBindingMatrix4(ManagedBindingId binding, Vector3 from, Vector3 to, Vector3 up)
        {
            return DecodeMatrix(Invoke(binding, UUID.Empty,
                                       Encode(from.x, from.y, from.z, to.x, to.y, to.z, up.x, up.y, up.z)));
        }

        internal static bool GetInputBoolean(ManagedBindingId binding, uint code) =>
            ReadBoolean(Invoke(binding, UUID.Empty, Encode(code)));

        internal static float GetBindingFloat(ManagedBindingId binding)
        {
            return GetBindingFloat(binding, UUID.Empty);
        }

        internal static Vector2 GetBindingVector2(ManagedBindingId binding)
        {
            byte[] value = Invoke(binding, UUID.Empty, Array.Empty<byte>());
            RequireLength(value, 8);
            return new Vector2(ReadSingle(value, 0), ReadSingle(value, 4));
        }

        private static T CreateComponent<T>(UUID entity) where T : Component
        {
#if CROWNY_MONO
            throw new NotSupportedException("Host-managed component proxies are unavailable in the Mono runtime.");
#else
            T component = Activator.CreateInstance<T>();
            component.m_ManagedEntity = new Entity { m_ManagedUuid = entity };
            return component;
#endif
        }

        private static byte[] Invoke(ManagedBindingId binding, UUID entity, byte[] input)
        {
            if (hostBindingHandler == null)
                throw new InvalidOperationException("Managed engine bindings are unavailable.");
            return hostBindingHandler(binding, entity, input ?? Array.Empty<byte>());
        }

        private static byte[] EncodeType(Type type) => Encoding.UTF8.GetBytes(type.FullName ?? type.Name);

        private static byte[] Encode(uint value) => BitConverter.GetBytes(value);

        private static byte[] Encode(UUID value)
        {
            byte[] result = new byte[16];
            WriteBigEndian(result, 0, value.d0);
            WriteBigEndian(result, 4, value.d1);
            WriteBigEndian(result, 8, value.d2);
            WriteBigEndian(result, 12, value.d3);
            return result;
        }

        private static byte[] Encode(Matrix4 value)
        {
            byte[] result = new byte[16 * sizeof(float)];
            for (int index = 0; index < 16; ++index)
                Buffer.BlockCopy(BitConverter.GetBytes(value[index]), 0, result, index * sizeof(float), sizeof(float));
            return result;
        }

        private static byte[] Encode(Vector2 value, int mode)
        {
            byte[] result = new byte[2 * sizeof(float) + sizeof(int)];
            Buffer.BlockCopy(BitConverter.GetBytes(value.x), 0, result, 0, sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(value.y), 0, result, sizeof(float), sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(mode), 0, result, 2 * sizeof(float), sizeof(int));
            return result;
        }

        private static byte[] Encode(Vector2 first, Vector2 second, int mode)
        {
            byte[] result = new byte[4 * sizeof(float) + sizeof(int)];
            Buffer.BlockCopy(BitConverter.GetBytes(first.x), 0, result, 0, sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(first.y), 0, result, sizeof(float), sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(second.x), 0, result, 2 * sizeof(float), sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(second.y), 0, result, 3 * sizeof(float), sizeof(float));
            Buffer.BlockCopy(BitConverter.GetBytes(mode), 0, result, 4 * sizeof(float), sizeof(int));
            return result;
        }

        private static byte[] Encode(params float[] values)
        {
            byte[] result = new byte[values.Length * sizeof(float)];
            for (int index = 0; index < values.Length; ++index)
                Buffer.BlockCopy(BitConverter.GetBytes(values[index]), 0, result, index * sizeof(float), sizeof(float));
            return result;
        }

        private static bool ReadBoolean(byte[] value)
        {
            RequireLength(value, 1);
            return value[0] != 0;
        }

        private static float ReadSingle(byte[] value, int offset) => BitConverter.ToSingle(value, offset);

        private static UUID DecodeUuid(byte[] value)
        {
            return new UUID(ReadBigEndian(value, 0), ReadBigEndian(value, 4), ReadBigEndian(value, 8), ReadBigEndian(value, 12));
        }

        private static Matrix4 DecodeMatrix(byte[] value)
        {
            RequireLength(value, 16 * sizeof(float));
            return new Matrix4(new Vector4(ReadSingle(value, 0), ReadSingle(value, 4), ReadSingle(value, 8), ReadSingle(value, 12)),
                               new Vector4(ReadSingle(value, 16), ReadSingle(value, 20), ReadSingle(value, 24), ReadSingle(value, 28)),
                               new Vector4(ReadSingle(value, 32), ReadSingle(value, 36), ReadSingle(value, 40), ReadSingle(value, 44)),
                               new Vector4(ReadSingle(value, 48), ReadSingle(value, 52), ReadSingle(value, 56), ReadSingle(value, 60)));
        }

        private static uint ReadBigEndian(byte[] value, int offset)
        {
            return (uint)value[offset] << 24 | (uint)value[offset + 1] << 16 | (uint)value[offset + 2] << 8 | value[offset + 3];
        }

        private static void WriteBigEndian(byte[] output, int offset, uint value)
        {
            output[offset] = (byte)(value >> 24);
            output[offset + 1] = (byte)(value >> 16);
            output[offset + 2] = (byte)(value >> 8);
            output[offset + 3] = (byte)value;
        }

        private static void RequireLength(byte[] value, int expected)
        {
            if (value == null || value.Length != expected)
                throw new InvalidOperationException($"The native host returned {value?.Length ?? 0} bytes; expected {expected}.");
        }

        internal struct CallbackScope : IDisposable
        {
            private readonly float previous;

            internal CallbackScope(float previousDeltaTime)
            {
                previous = previousDeltaTime;
            }

            public void Dispose()
            {
                callbackDeltaTime = previous;
            }
        }
    }
}
