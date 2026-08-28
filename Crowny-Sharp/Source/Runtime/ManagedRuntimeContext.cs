using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Crowny
{
    internal static unsafe partial class ManagedRuntimeContext
    {
        [ThreadStatic]
        private static float callbackDeltaTime;
        private static Action<int, string> logHandler;
        private static Func<UUID, Type, Component> scriptResolver;

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

        internal static void SetNativeHostApi(ManagedNativeHostApi api)
        {
            ManagedHostTransport.SetApi(api);
        }

        internal static void SetScriptResolver(Func<UUID, Type, Component> resolver)
        {
            scriptResolver = resolver;
        }

#if !CROWNY_MONO
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
            return EntityHasComponent(entity, type.FullName ?? type.Name);
        }

        internal static T AddComponent<T>(UUID entity) where T : Component
        {
            Type type = typeof(T);
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
                throw new InvalidOperationException("Managed script components must be attached through scene script metadata.");
            EntityAddComponent(entity, type.FullName ?? type.Name);
            return CreateComponent<T>(entity);
        }

        internal static void RemoveComponent<T>(UUID entity) where T : Component
        {
            Type type = typeof(T);
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
                throw new InvalidOperationException("Managed script components must be removed through scene script metadata.");
            EntityRemoveComponent(entity, type.FullName ?? type.Name);
        }

        internal static T CreateAsset<T>(UUID uuid) where T : Asset
        {
            if (uuid == UUID.Empty)
                return null;
            T asset = Activator.CreateInstance<T>();
            asset.m_ManagedUuid = uuid;
            return asset;
        }

        private static T CreateComponent<T>(UUID entity) where T : Component
        {
            T component = Activator.CreateInstance<T>();
            component.m_ManagedEntity = new Entity { m_ManagedUuid = entity };
            return component;
        }
#endif

        private static void EnsureHostBindings()
        {
#if CROWNY_MONO
            if (!ManagedHostTransport.IsInitialized)
            {
                IntPtr hostApi = Internal_GetNativeHostApi();
                if (hostApi != IntPtr.Zero)
                    ManagedHostTransport.SetApi((ManagedNativeHostApi)Marshal.PtrToStructure(hostApi, typeof(ManagedNativeHostApi)));
            }
#endif
            if (!ManagedHostTransport.IsInitialized)
                throw new InvalidOperationException("Managed native host bindings are unavailable.");
        }

        private static ManagedNativeUuid EncodeUuid(UUID value)
        {
            ManagedNativeUuid result = default;
            byte* bytes = result.Bytes;
            WriteBigEndian(bytes, 0, value.d0);
            WriteBigEndian(bytes, 4, value.d1);
            WriteBigEndian(bytes, 8, value.d2);
            WriteBigEndian(bytes, 12, value.d3);
            return result;
        }

        private static UUID DecodeUuid(ManagedNativeUuid value)
        {
            byte* bytes = value.Bytes;
            return new UUID(ReadBigEndian(bytes, 0), ReadBigEndian(bytes, 4), ReadBigEndian(bytes, 8), ReadBigEndian(bytes, 12));
        }

        private static string DecodeString(ManagedNativeStringView value)
        {
            if (value.Data == null || value.Length == 0)
                return string.Empty;
            return Encoding.UTF8.GetString(value.Data, checked((int)value.Length));
        }

        private static CharacterInfo DecodeFontCharacterInfo(ManagedNativeFontCharacterInfo value)
        {
            return new CharacterInfo
            {
                sourceFont = DecodeUuid(value.SourceFont),
                requestedCodePoint = value.RequestedCodePoint,
                resolvedCodePoint = value.ResolvedCodePoint,
                glyphIndex = value.GlyphIndex,
                advance = value.Advance,
                planeLeft = value.PlaneLeft,
                planeBottom = value.PlaneBottom,
                planeRight = value.PlaneRight,
                planeTop = value.PlaneTop,
                atlasLeft = value.AtlasLeft,
                atlasBottom = value.AtlasBottom,
                atlasRight = value.AtlasRight,
                atlasTop = value.AtlasTop,
                whitespace = value.Whitespace != 0,
                valid = value.Valid != 0,
            };
        }

        private static ManagedNativeMatrix4 EncodeMatrix(Matrix4 value)
        {
            ManagedNativeMatrix4 result = default;
            float* values = result.Values;
            for (int index = 0; index < 16; ++index)
                values[index] = value[index];
            return result;
        }

        private static Matrix4 DecodeMatrix(ManagedNativeMatrix4 value)
        {
            float* values = value.Values;
            return new Matrix4(new Vector4(values[0], values[1], values[2], values[3]),
                               new Vector4(values[4], values[5], values[6], values[7]),
                               new Vector4(values[8], values[9], values[10], values[11]),
                               new Vector4(values[12], values[13], values[14], values[15]));
        }

        private static uint ReadBigEndian(byte* value, int offset)
        {
            return (uint)value[offset] << 24 | (uint)value[offset + 1] << 16 | (uint)value[offset + 2] << 8 | value[offset + 3];
        }

        private static void WriteBigEndian(byte* output, int offset, uint value)
        {
            output[offset] = (byte)(value >> 24);
            output[offset + 1] = (byte)(value >> 16);
            output[offset + 2] = (byte)(value >> 8);
            output[offset + 3] = (byte)value;
        }

        private static void EnsureStatus(int status, string operation)
        {
            if (status != 0)
                throw new InvalidOperationException($"The native host could not complete {operation}. Status {status}.");
        }

#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern IntPtr Internal_GetNativeHostApi();
#endif

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
