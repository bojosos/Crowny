using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;

namespace Crowny
{
    internal sealed class ScriptMember
    {
        private readonly MemberInfo member;

        internal ScriptMember(FieldInfo field)
        {
            member = field;
            Name = field.Name;
            ValueType = field.FieldType;
            bool publicMember = field.IsPublic;
            IsSerializable = ScriptMetadata.CanSerialize(ValueType) && !Attribute.IsDefined(field, typeof(DontSerializeField)) &&
                             (publicMember || Attribute.IsDefined(field, typeof(SerializeField)));
            IsInspectable = ScriptMetadata.CanInspect(ValueType) && !Attribute.IsDefined(field, typeof(HideInInspector)) &&
                             (publicMember || Attribute.IsDefined(field, typeof(ShowInInspector)));
            CanWrite = !field.IsInitOnly;
            IsReadOnly = !CanWrite || Attribute.IsDefined(field, typeof(ReadOnly));
        }

        internal ScriptMember(PropertyInfo property)
        {
            member = property;
            Name = property.Name;
            ValueType = property.PropertyType;
            MethodInfo getter = property.GetGetMethod(true);
            MethodInfo setter = property.GetSetMethod(true);
            bool hasPublicAccessor = getter != null && getter.IsPublic || setter != null && setter.IsPublic;
            IsSerializable = ScriptMetadata.CanSerialize(ValueType) && !Attribute.IsDefined(property, typeof(DontSerializeField)) &&
                             (hasPublicAccessor || Attribute.IsDefined(property, typeof(SerializeField)));
            IsInspectable = ScriptMetadata.CanInspect(ValueType) && !Attribute.IsDefined(property, typeof(HideInInspector)) &&
                             (hasPublicAccessor || Attribute.IsDefined(property, typeof(ShowInInspector)));
            CanWrite = setter != null;
            IsReadOnly = !CanWrite || Attribute.IsDefined(property, typeof(ReadOnly));
        }

        internal string Name { get; private set; }
        internal Type ValueType { get; private set; }
        internal bool IsSerializable { get; private set; }
        internal bool IsInspectable { get; private set; }
        internal bool CanWrite { get; private set; }
        internal bool IsReadOnly { get; private set; }

        internal object GetValue(object instance)
        {
            FieldInfo field = member as FieldInfo;
            if (field != null)
                return field.GetValue(instance);
            PropertyInfo property = member as PropertyInfo;
            if (property != null)
                return property.GetValue(instance, null);
            throw new InvalidOperationException("Unsupported managed member " + member + ".");
        }

        internal void SetValue(object instance, object value)
        {
            if (!CanWrite)
                return;
            FieldInfo field = member as FieldInfo;
            if (field != null)
            {
                field.SetValue(instance, value);
                return;
            }
            PropertyInfo property = member as PropertyInfo;
            if (property != null)
            {
                property.SetValue(instance, value, null);
                return;
            }
            throw new InvalidOperationException("Unsupported managed member " + member + ".");
        }

    }

    internal static class ScriptMetadata
    {
        internal static ScriptMember[] Discover(Type type, bool scriptType)
        {
            Stack<Type> hierarchy = new Stack<Type>();
            Type stopType = scriptType ? typeof(EntityBehaviour) : typeof(object);
            for (Type current = type; current != null && current != stopType && current != typeof(object); current = current.BaseType)
                hierarchy.Push(current);

            List<MemberCandidate> members = new List<MemberCandidate>();
            int typeOrder = 0;
            while (hierarchy.Count != 0)
            {
                Type current = hierarchy.Pop();
                foreach (FieldInfo field in current.GetFields(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public |
                                                               BindingFlags.NonPublic))
                {
                    if (field.IsStatic)
                        continue;
                    ScriptMember member = new ScriptMember(field);
                    if (member.IsSerializable || member.IsInspectable)
                        members.Add(new MemberCandidate(typeOrder, 0, field.MetadataToken, member));
                }
                foreach (PropertyInfo property in current.GetProperties(BindingFlags.DeclaredOnly | BindingFlags.Instance |
                                                                         BindingFlags.Public | BindingFlags.NonPublic))
                {
                    MethodInfo getter = property.GetGetMethod(true);
                    MethodInfo setter = property.GetSetMethod(true);
                    if (getter == null || getter.IsStatic || setter != null && setter.IsStatic || property.GetIndexParameters().Length != 0)
                        continue;
                    ScriptMember member = new ScriptMember(property);
                    if (member.IsSerializable || member.IsInspectable)
                        members.Add(new MemberCandidate(typeOrder, 1, property.MetadataToken, member));
                }
                ++typeOrder;
            }

            ScriptMember[] result = members.OrderBy(candidate => candidate.TypeOrder)
                                           .ThenBy(candidate => candidate.KindOrder)
                                           .ThenBy(candidate => candidate.MetadataToken)
                                           .Select(candidate => candidate.Member)
                                           .ToArray();
            HashSet<string> names = new HashSet<string>(StringComparer.Ordinal);
            foreach (ScriptMember scriptMember in result)
            {
                if (!names.Add(scriptMember.Name))
                    throw new InvalidOperationException("Managed type " + type.FullName +
                                                        " has more than one serializable member named " + scriptMember.Name + ".");
            }
            return result;
        }

        internal static string ValueKind(Type declaredType)
        {
            Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
            if (type == typeof(bool)) return "Boolean";
            if (type == typeof(string) || type == typeof(char)) return "String";
            if (type.IsEnum) return "Enum";
            if (type == typeof(float) || type == typeof(double)) return "Float";
            if (type == typeof(decimal)) return "Decimal";
            if (type == typeof(byte) || type == typeof(ushort) || type == typeof(uint) || type == typeof(ulong))
                return "UnsignedInteger";
            if (type == typeof(sbyte) || type == typeof(short) || type == typeof(int) || type == typeof(long))
                return "SignedInteger";
            if (type == typeof(UUID)) return "Uuid";
            if (type == typeof(Vector2)) return "Vector2";
            if (type == typeof(Vector3)) return "Vector3";
            if (type == typeof(Vector4)) return "Vector4";
            if (type == typeof(Color)) return "Color";
            if (type == typeof(Quaternion)) return "Quaternion";
            if (type == typeof(Matrix4)) return "Matrix4";
            if (typeof(Entity).IsAssignableFrom(type)) return "Entity";
            if (typeof(Component).IsAssignableFrom(type)) return "Component";
            if (typeof(Asset).IsAssignableFrom(type)) return "Asset";
            if (type.IsArray) return "Array";
            if (IsGeneric(type, typeof(List<>))) return "List";
            if (IsGeneric(type, typeof(Dictionary<,>))) return "Dictionary";
            return "Object";
        }

        internal static bool CanSerialize(Type declaredType)
        {
            Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
            if (type.IsEnum || type == typeof(bool) || type == typeof(char) || type == typeof(sbyte) || type == typeof(byte) ||
                type == typeof(short) || type == typeof(ushort) || type == typeof(int) || type == typeof(uint) ||
                type == typeof(long) || type == typeof(ulong) || type == typeof(float) || type == typeof(double) ||
                type == typeof(decimal) || type == typeof(string) || type == typeof(UUID) || type == typeof(Vector2) ||
                type == typeof(Vector3) || type == typeof(Vector4) || type == typeof(Color) || type == typeof(Quaternion) ||
                type == typeof(Matrix4) ||
                typeof(Entity).IsAssignableFrom(type) || typeof(Asset).IsAssignableFrom(type))
                return true;
            if (type.IsArray)
                return CanSerialize(type.GetElementType());
            if (IsGeneric(type, typeof(List<>)))
                return CanSerialize(type.GetGenericArguments()[0]);
            if (IsGeneric(type, typeof(Dictionary<,>)))
            {
                Type[] arguments = type.GetGenericArguments();
                return CanSerialize(arguments[0]) && CanSerialize(arguments[1]);
            }
            return Attribute.IsDefined(type, typeof(SerializeObject)) || typeof(Component).IsAssignableFrom(type);
        }

        internal static bool CanInspect(Type declaredType)
        {
            Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
            if (CanSerialize(type))
                return true;
            if (type.IsArray)
                return CanInspect(type.GetElementType());
            if (IsGeneric(type, typeof(List<>)))
                return CanInspect(type.GetGenericArguments()[0]);
            if (IsGeneric(type, typeof(Dictionary<,>)))
            {
                Type[] arguments = type.GetGenericArguments();
                return CanInspect(arguments[0]) && CanInspect(arguments[1]);
            }
            return Attribute.IsDefined(type, typeof(ShowInInspector));
        }

        private static bool IsGeneric(Type type, Type definition)
        {
            return type.IsGenericType && type.GetGenericTypeDefinition() == definition;
        }

        private sealed class MemberCandidate
        {
            internal MemberCandidate(int typeOrder, int kindOrder, int metadataToken, ScriptMember member)
            {
                TypeOrder = typeOrder;
                KindOrder = kindOrder;
                MetadataToken = metadataToken;
                Member = member;
            }

            internal int TypeOrder { get; private set; }
            internal int KindOrder { get; private set; }
            internal int MetadataToken { get; private set; }
            internal ScriptMember Member { get; private set; }
        }
    }

    internal static class ScriptCallbacks
    {
        private static readonly CallbackSignature[] signatures =
        {
            new CallbackSignature("Start", "Start", null, null),
            new CallbackSignature("Update", "Update", null, null),
            new CallbackSignature("FixedUpdate", "FixedUpdate", null, null),
            new CallbackSignature("Destroy", "Destroy", "OnDestroy", null),
            new CallbackSignature("CollisionEnter2D", "OnCollisionEnter2D", null, typeof(Collision2D)),
            new CallbackSignature("CollisionStay2D", "OnCollisionStay2D", null, typeof(Collision2D)),
            new CallbackSignature("CollisionExit2D", "OnCollisionExit2D", null, typeof(Collision2D)),
            new CallbackSignature("TriggerEnter2D", "OnTriggerEnter2D", null, typeof(Entity)),
            new CallbackSignature("TriggerStay2D", "OnTriggerStay2D", null, typeof(Entity)),
            new CallbackSignature("TriggerExit2D", "OnTriggerExit2D", null, typeof(Entity)),
            new CallbackSignature("CollisionEnter3D", "OnCollisionEnter3D", "OnCollisionEnter", typeof(Collision3D)),
            new CallbackSignature("CollisionStay3D", "OnCollisionStay3D", "OnCollisionStay", typeof(Collision3D)),
            new CallbackSignature("CollisionExit3D", "OnCollisionExit3D", "OnCollisionExit", typeof(Collision3D)),
            new CallbackSignature("TriggerEnter3D", "OnTriggerEnter3D", "OnTriggerEnter", typeof(Entity)),
            new CallbackSignature("TriggerStay3D", "OnTriggerStay3D", "OnTriggerStay", typeof(Entity)),
            new CallbackSignature("TriggerExit3D", "OnTriggerExit3D", "OnTriggerExit", typeof(Entity))
        };

        internal static Dictionary<string, MethodInfo> Discover(Type type)
        {
            Dictionary<string, MethodInfo> callbacks = new Dictionary<string, MethodInfo>(StringComparer.Ordinal);
            foreach (CallbackSignature signature in signatures)
            {
                MethodInfo callback = Find(type, signature.MethodName, signature.ParameterType);
                if (callback == null && signature.FallbackName != null)
                    callback = Find(type, signature.FallbackName, signature.ParameterType);
                callbacks.Add(signature.Kind, callback);
            }
            return callbacks;
        }

        private static MethodInfo Find(Type type, string methodName, Type parameterType)
        {
            for (Type current = type; current != null && current != typeof(EntityBehaviour); current = current.BaseType)
            {
                foreach (MethodInfo method in current.GetMethods(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public |
                                                                  BindingFlags.NonPublic))
                {
                    if (method.Name != methodName || method.ContainsGenericParameters)
                        continue;
                    ParameterInfo[] parameters = method.GetParameters();
                    if (parameterType == null ? parameters.Length != 0
                                              : parameters.Length != 1 || parameters[0].ParameterType != parameterType)
                        continue;
                    if (method.ReturnType != typeof(void))
                        throw new InvalidOperationException("Managed callback " + current.FullName + "." + methodName +
                                                            " must return void.");
                    return method;
                }
            }
            return null;
        }

        private sealed class CallbackSignature
        {
            internal CallbackSignature(string kind, string methodName, string fallbackName, Type parameterType)
            {
                Kind = kind;
                MethodName = methodName;
                FallbackName = fallbackName;
                ParameterType = parameterType;
            }

            internal string Kind { get; private set; }
            internal string MethodName { get; private set; }
            internal string FallbackName { get; private set; }
            internal Type ParameterType { get; private set; }
        }
    }

    internal static class ManagedScriptCatalog
    {
        internal static string CaptureLoadedAssembly(string assemblyName)
        {
            Assembly target = null;
            foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                if (string.Equals(assembly.GetName().Name, assemblyName, StringComparison.Ordinal))
                {
                    target = assembly;
                    break;
                }
            }
            if (target == null)
                throw new InvalidOperationException("Managed assembly " + assemblyName + " is not loaded.");
            return Capture(target.GetTypes().Where(type => !type.IsAbstract && typeof(EntityBehaviour).IsAssignableFrom(type)));
        }

        internal static string Capture(IEnumerable<Type> scriptTypes)
        {
            Type[] orderedTypes = scriptTypes.OrderBy(type => type.FullName, StringComparer.Ordinal).ToArray();
            List<object> types = new List<object>();
            foreach (Type type in orderedTypes)
                types.Add(WriteType(type));
            Dictionary<string, object> catalog = new Dictionary<string, object>(StringComparer.Ordinal);
            catalog.Add("ManifestVersion", 2);
            catalog.Add("Types", types);
            return ManagedJsonCodec.SerializeDom(catalog);
        }

        private static Dictionary<string, object> WriteType(Type type)
        {
            Dictionary<string, MethodInfo> callbacks = ScriptCallbacks.Discover(type);
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("StableId", StableId((type.Assembly.GetName().Name ?? string.Empty) + ":" + type.FullName));
            result.Add("Assembly", type.Assembly.GetName().Name ?? string.Empty);
            result.Add("Namespace", type.Namespace ?? string.Empty);
            result.Add("TypeName", TypeName(type));
            result.Add("BaseType", type.BaseType != null && type.BaseType != typeof(EntityBehaviour) ? Identity(type.BaseType) : null);
            result.Add("RunInEditor", Attribute.IsDefined(type, typeof(RunInEditor), true));
            result.Add("Events", callbacks.Where(callback => callback.Value != null)
                                          .OrderBy(callback => callback.Key, StringComparer.Ordinal)
                                          .Select(callback => callback.Key)
                                          .ToArray());

            List<object> fields = new List<object>();
            foreach (ScriptMember member in ScriptMetadata.Discover(type, true))
            {
                Dictionary<string, object> field = new Dictionary<string, object>(StringComparer.Ordinal);
                field.Add("StableId", StableId((type.Assembly.GetName().Name ?? string.Empty) + ":" + type.FullName + ":" + member.Name));
                field.Add("Name", member.Name);
                field.Add("ValueKind", ScriptMetadata.ValueKind(member.ValueType));
                field.Add("ElementKind", ElementKind(member.ValueType));
                field.Add("KeyKind", KeyKind(member.ValueType));
                field.Add("DeclaredType", DeclaredType(member.ValueType));
                field.Add("IsNullable", Nullable.GetUnderlyingType(member.ValueType) != null || !member.ValueType.IsValueType);
                field.Add("IsSerializable", member.IsSerializable);
                field.Add("IsInspectable", member.IsInspectable);
                field.Add("IsReadOnly", member.IsReadOnly);
                fields.Add(field);
            }
            result.Add("Fields", fields);
            return result;
        }

        private static Dictionary<string, object> Identity(Type type)
        {
            Dictionary<string, object> identity = new Dictionary<string, object>(StringComparer.Ordinal);
            identity.Add("Assembly", type.Assembly.GetName().Name ?? string.Empty);
            identity.Add("Namespace", type.Namespace ?? string.Empty);
            identity.Add("TypeName", TypeName(type));
            return identity;
        }

        private static string TypeName(Type type)
        {
            string fullName = type.FullName ?? type.Name;
            string typeNamespace = type.Namespace ?? string.Empty;
            return typeNamespace.Length == 0 ? fullName : fullName.Substring(typeNamespace.Length + 1);
        }

        private static ulong StableId(string text)
        {
            const ulong offset = 14695981039346656037UL;
            const ulong prime = 1099511628211UL;
            ulong hash = offset;
            foreach (byte value in Encoding.UTF8.GetBytes(text ?? string.Empty))
                hash = (hash ^ value) * prime;
            return hash == 0 ? 1 : hash;
        }

        private static string ElementKind(Type type)
        {
            if (type.IsArray)
                return ScriptMetadata.ValueKind(type.GetElementType());
            if (IsGeneric(type, typeof(List<>)) || IsGeneric(type, typeof(Dictionary<,>)))
                return ScriptMetadata.ValueKind(type.GetGenericArguments()[type.GetGenericArguments().Length - 1]);
            return null;
        }

        private static string KeyKind(Type type)
        {
            return IsGeneric(type, typeof(Dictionary<,>)) ? ScriptMetadata.ValueKind(type.GetGenericArguments()[0]) : null;
        }

        private static Dictionary<string, object> DeclaredType(Type declaredType)
        {
            Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
            string kind = ScriptMetadata.ValueKind(type);
            return kind == "Object" || kind == "Component" ? Identity(type) : null;
        }

        private static bool IsGeneric(Type type, Type definition)
        {
            return type.IsGenericType && type.GetGenericTypeDefinition() == definition;
        }
    }
}
