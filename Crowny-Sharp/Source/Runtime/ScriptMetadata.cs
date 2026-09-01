using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;

namespace Crowny
{
    internal enum ScriptConditionEffect
    {
        Show,
        Hide,
        Enable,
        Disable
    }

    internal sealed class ScriptConditionDescriptor
    {
        private readonly Func<object, object> accessor;

        internal ScriptConditionDescriptor(ConditionalAttribute attribute, ScriptConditionEffect effect, Func<object, object> accessor)
        {
            Attribute = attribute;
            Effect = effect;
            this.accessor = accessor;
        }

        internal ConditionalAttribute Attribute { get; private set; }
        internal ScriptConditionEffect Effect { get; private set; }

        internal bool Evaluate(object instance)
        {
            object value = accessor(instance);
            if (Attribute.HasValue)
                return ValuesEqual(value, Attribute.Value);
            if (value == null)
                return false;
            if (value is bool)
                return (bool)value;
            Type type = value.GetType();
            if (type.IsEnum)
            {
                Type underlying = Enum.GetUnderlyingType(type);
                return underlying == typeof(byte) || underlying == typeof(ushort) || underlying == typeof(uint) ||
                       underlying == typeof(ulong)
                           ? Convert.ToUInt64(value) != 0
                           : Convert.ToInt64(value) != 0;
            }
            if (value is string)
                return ((string)value).Length != 0;
            if (value is IConvertible)
            {
                try { return Convert.ToDouble(value) != 0.0; }
                catch (FormatException) { }
                catch (InvalidCastException) { }
                catch (OverflowException) { }
            }
            return true;
        }

        private static bool ValuesEqual(object actual, object expected)
        {
            if (object.Equals(actual, expected))
                return true;
            if (actual == null || expected == null)
                return false;

            Type actualType = actual.GetType();
            Type expectedType = expected.GetType();
            if (actualType.IsEnum)
            {
                actual = Convert.ChangeType(actual, Enum.GetUnderlyingType(actualType));
                actualType = actual.GetType();
            }
            if (expectedType.IsEnum)
            {
                expected = Convert.ChangeType(expected, Enum.GetUnderlyingType(expectedType));
                expectedType = expected.GetType();
            }
            if (!IsNumeric(actualType) || !IsNumeric(expectedType))
                return false;
            try { return Convert.ToDecimal(actual) == Convert.ToDecimal(expected); }
            catch (OverflowException) { return Convert.ToDouble(actual) == Convert.ToDouble(expected); }
        }

        private static bool IsNumeric(Type type)
        {
            return type == typeof(byte) || type == typeof(sbyte) || type == typeof(short) || type == typeof(ushort) ||
                   type == typeof(int) || type == typeof(uint) || type == typeof(long) || type == typeof(ulong) ||
                   type == typeof(float) || type == typeof(double) || type == typeof(decimal);
        }
    }

    internal sealed class ScriptValueChangedDescriptor
    {
        internal ScriptValueChangedDescriptor(OnValueChanged attribute, MethodInfo method, ulong stableId, bool passValue)
        {
            Attribute = attribute;
            Method = method;
            StableId = stableId;
            PassValue = passValue;
        }

        internal OnValueChanged Attribute { get; private set; }
        internal MethodInfo Method { get; private set; }
        internal ulong StableId { get; private set; }
        internal bool PassValue { get; private set; }
    }

    internal sealed class ScriptMember
    {
        private readonly MemberInfo member;

        internal ScriptMember(FieldInfo field)
        {
            member = field;
            Name = field.Name;
            ValueType = field.FieldType;
            SearchSettings = ResolveSearchable(field, ValueType);
            ProgressBarSettings = ResolveProgressBar(field, ValueType);
            FilePathSettings = ResolveAttribute<FilePath>(field, ValueType, typeof(string));
            FolderPathSettings = ResolveAttribute<FolderPath>(field, ValueType, typeof(string));
            MultilineSettings = ResolveAttribute<Multiline>(field, ValueType, typeof(string));
            ColorUsageSettings = ResolveAttribute<ColorUsage>(field, ValueType, typeof(Color));
            DictionaryDisplaySettings = ResolveDictionaryDisplay(field, ValueType);
            LabelText = ResolveLabel(field);
            TooltipText = ResolveTooltip(field, ValueType);
            EnumButtons enumButtonsSettings;
            Type enumButtonsType;
            ResolveEnumButtons(field, ValueType, out enumButtonsSettings, out enumButtonsType);
            EnumButtonsSettings = enumButtonsSettings;
            EnumButtonsType = enumButtonsType;
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
            SearchSettings = ResolveSearchable(property, ValueType);
            ProgressBarSettings = ResolveProgressBar(property, ValueType);
            FilePathSettings = ResolveAttribute<FilePath>(property, ValueType, typeof(string));
            FolderPathSettings = ResolveAttribute<FolderPath>(property, ValueType, typeof(string));
            MultilineSettings = ResolveAttribute<Multiline>(property, ValueType, typeof(string));
            ColorUsageSettings = ResolveAttribute<ColorUsage>(property, ValueType, typeof(Color));
            DictionaryDisplaySettings = null;
            LabelText = ResolveLabel(property);
            TooltipText = ResolveTooltip(property, ValueType);
            EnumButtons enumButtonsSettings;
            Type enumButtonsType;
            ResolveEnumButtons(property, ValueType, out enumButtonsSettings, out enumButtonsType);
            EnumButtonsSettings = enumButtonsSettings;
            EnumButtonsType = enumButtonsType;
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
        internal Searchable SearchSettings { get; private set; }
        internal ProgressBar ProgressBarSettings { get; private set; }
        internal FilePath FilePathSettings { get; private set; }
        internal FolderPath FolderPathSettings { get; private set; }
        internal Multiline MultilineSettings { get; private set; }
        internal ColorUsage ColorUsageSettings { get; private set; }
        internal EnumButtons EnumButtonsSettings { get; private set; }
        internal Type EnumButtonsType { get; private set; }
        internal DictionaryDisplay DictionaryDisplaySettings { get; private set; }
        internal string LabelText { get; private set; }
        internal string TooltipText { get; private set; }
        internal ScriptConditionDescriptor[] Conditions { get; private set; } = new ScriptConditionDescriptor[0];
        internal ScriptValueChangedDescriptor[] ValueChangedActions { get; private set; } = new ScriptValueChangedDescriptor[0];

        internal void BindInspectorResolvers(Type ownerType)
        {
            List<ScriptConditionDescriptor> conditions = new List<ScriptConditionDescriptor>();
            AddConditions<ShowIf>(conditions, ownerType, ScriptConditionEffect.Show);
            AddConditions<HideIf>(conditions, ownerType, ScriptConditionEffect.Hide);
            AddConditions<EnableIf>(conditions, ownerType, ScriptConditionEffect.Enable);
            AddConditions<DisableIf>(conditions, ownerType, ScriptConditionEffect.Disable);
            Conditions = conditions.ToArray();

            List<ScriptValueChangedDescriptor> actions = new List<ScriptValueChangedDescriptor>();
            foreach (OnValueChanged attribute in member.GetCustomAttributes(typeof(OnValueChanged), true).Cast<OnValueChanged>())
            {
                string actionName = attribute.Action.EndsWith("()", StringComparison.Ordinal)
                                      ? attribute.Action.Substring(0, attribute.Action.Length - 2)
                                      : attribute.Action;
                MethodInfo method = FindMethod(ownerType, actionName, ValueType);
                if (method == null)
                {
                    Debug.Warn("Ignoring OnValueChanged action " + attribute.Action + " on " + ownerType.FullName + "." + Name +
                               " because it must resolve to a void method with zero parameters or one compatible value parameter.");
                    continue;
                }
                actions.Add(new ScriptValueChangedDescriptor(attribute, method, ScriptButtonMetadata.MethodStableId(ownerType, method),
                                                             method.GetParameters().Length == 1));
            }
            ValueChangedActions = actions.ToArray();
        }

        private void AddConditions<T>(List<ScriptConditionDescriptor> output, Type ownerType, ScriptConditionEffect effect)
          where T : ConditionalAttribute
        {
            foreach (T attribute in member.GetCustomAttributes(typeof(T), true).Cast<T>())
            {
                Func<object, object> accessor = FindAccessor(ownerType, attribute.Condition);
                if (accessor == null)
                {
                    Debug.Warn("Ignoring " + typeof(T).Name + " condition " + attribute.Condition + " on " + ownerType.FullName + "." + Name +
                               " because it does not resolve to a field, readable property, or parameterless method.");
                    continue;
                }
                output.Add(new ScriptConditionDescriptor(attribute, effect, accessor));
            }
        }

        private static Func<object, object> FindAccessor(Type ownerType, string name)
        {
            if (name.StartsWith("@", StringComparison.Ordinal))
                return null;
            const BindingFlags flags = BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Static |
                                       BindingFlags.Public | BindingFlags.NonPublic;
            for (Type current = ownerType; current != null && current != typeof(object); current = current.BaseType)
            {
                FieldInfo field = current.GetField(name, flags);
                if (field != null)
                    return instance => field.GetValue(field.IsStatic ? null : instance);
                PropertyInfo property = current.GetProperty(name, flags);
                MethodInfo getter = property == null ? null : property.GetGetMethod(true);
                if (getter != null && property.GetIndexParameters().Length == 0)
                    return instance => property.GetValue(getter.IsStatic ? null : instance, null);
                MethodInfo method = current.GetMethods(flags).FirstOrDefault(candidate => candidate.Name == name &&
                                                                              !candidate.ContainsGenericParameters &&
                                                                              candidate.ReturnType != typeof(void) &&
                                                                              candidate.GetParameters().Length == 0);
                if (method != null)
                    return instance => method.Invoke(method.IsStatic ? null : instance, null);
            }
            return null;
        }

        private static MethodInfo FindMethod(Type ownerType, string name, Type valueType)
        {
            const BindingFlags flags = BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Static |
                                       BindingFlags.Public | BindingFlags.NonPublic;
            for (Type current = ownerType; current != null && current != typeof(object); current = current.BaseType)
            {
                foreach (MethodInfo method in current.GetMethods(flags).Where(candidate => candidate.Name == name))
                {
                    if (method.ContainsGenericParameters || method.ReturnType != typeof(void))
                        continue;
                    ParameterInfo[] parameters = method.GetParameters();
                    if (parameters.Length == 0 || parameters.Length == 1 && parameters[0].ParameterType.IsAssignableFrom(valueType))
                        return method;
                }
            }
            return null;
        }

        private static string ResolveLabel(MemberInfo memberInfo)
        {
            Label label = memberInfo.GetCustomAttributes(typeof(Label), true).FirstOrDefault() as Label;
            return label == null ? null : label.label;
        }

        private static string ResolveTooltip(MemberInfo memberInfo, Type valueType)
        {
            Tooltip tooltip = memberInfo.GetCustomAttributes(typeof(Tooltip), true).FirstOrDefault() as Tooltip;
            Type type = Nullable.GetUnderlyingType(valueType) ?? valueType;
            if (tooltip == null && type.IsEnum)
                tooltip = type.GetCustomAttributes(typeof(Tooltip), true).FirstOrDefault() as Tooltip;
            return tooltip == null ? null : tooltip.tooltip;
        }

        private static DictionaryDisplay ResolveDictionaryDisplay(FieldInfo field, Type valueType)
        {
            if (!valueType.IsGenericType || valueType.GetGenericTypeDefinition() != typeof(Dictionary<,>))
                return null;
            return field.GetCustomAttributes(typeof(DictionaryDisplay), true).FirstOrDefault() as DictionaryDisplay;
        }

        private static void ResolveEnumButtons(MemberInfo memberInfo, Type valueType, out EnumButtons settings, out Type enumType)
        {
            settings = null;
            enumType = Nullable.GetUnderlyingType(valueType) ?? valueType;
            if (enumType.IsArray)
                enumType = Nullable.GetUnderlyingType(enumType.GetElementType()) ?? enumType.GetElementType();
            else if (enumType.IsGenericType && enumType.GetGenericTypeDefinition() == typeof(List<>))
            {
                Type elementType = enumType.GetGenericArguments()[0];
                enumType = Nullable.GetUnderlyingType(elementType) ?? elementType;
            }

            if (!enumType.IsEnum)
            {
                enumType = null;
                return;
            }

            settings = memberInfo.GetCustomAttributes(typeof(EnumButtons), true).FirstOrDefault() as EnumButtons;
            if (settings == null && Attribute.IsDefined(enumType, typeof(EnumQuickTabs), true))
                settings = new EnumButtons();
            if (settings == null)
                enumType = null;
        }

        private static T ResolveAttribute<T>(MemberInfo memberInfo, Type valueType, Type requiredType) where T : Attribute
        {
            Type type = Nullable.GetUnderlyingType(valueType) ?? valueType;
            if (type != requiredType)
                return null;
            return memberInfo.GetCustomAttributes(typeof(T), true).FirstOrDefault() as T;
        }

        private static ProgressBar ResolveProgressBar(MemberInfo memberInfo, Type valueType)
        {
            Type type = Nullable.GetUnderlyingType(valueType) ?? valueType;
            string kind = ScriptMetadata.ValueKind(type);
            if (kind != "SignedInteger" && kind != "UnsignedInteger" && kind != "Float" && kind != "Decimal")
                return null;
            return memberInfo.GetCustomAttributes(typeof(ProgressBar), true).FirstOrDefault() as ProgressBar;
        }

        private static Searchable ResolveSearchable(MemberInfo memberInfo, Type valueType)
        {
            Searchable direct = memberInfo.GetCustomAttributes(typeof(Searchable), true).FirstOrDefault() as Searchable;
            if (direct != null)
                return direct;

            Type candidate = Nullable.GetUnderlyingType(valueType) ?? valueType;
            while (candidate != null)
            {
                Searchable typeAttribute = candidate.GetCustomAttributes(typeof(Searchable), true).FirstOrDefault() as Searchable;
                if (typeAttribute != null)
                    return typeAttribute;
                if (candidate.IsArray)
                    candidate = candidate.GetElementType();
                else if (candidate.IsGenericType &&
                         (candidate.GetGenericTypeDefinition() == typeof(List<>) || candidate.GetGenericTypeDefinition() == typeof(Dictionary<,>)))
                    candidate = candidate.GetGenericArguments()[candidate.GetGenericArguments().Length - 1];
                else
                    candidate = null;
            }
            return null;
        }

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
        private static readonly ConditionalWeakTable<Type, ScriptMember[]> scriptCache =
            new ConditionalWeakTable<Type, ScriptMember[]>();
        private static readonly ConditionalWeakTable<Type, ScriptMember[]> objectCache =
            new ConditionalWeakTable<Type, ScriptMember[]>();

        internal static ScriptMember[] Discover(Type type, bool scriptType)
        {
            return (scriptType ? scriptCache : objectCache).GetValue(type, key => DiscoverUncached(key, scriptType));
        }

        private static ScriptMember[] DiscoverUncached(Type type, bool scriptType)
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
                    member.BindInspectorResolvers(type);
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
                    member.BindInspectorResolvers(type);
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

    internal sealed class ScriptButtonMethod
    {
        internal ScriptButtonMethod(MethodInfo method, Button settings, ulong stableId)
        {
            Method = method;
            Settings = settings;
            StableId = stableId;
            Parameters = method.GetParameters();
            DisplayName = string.IsNullOrEmpty(settings.Name) ? NiceName(method.Name) : settings.Name;
        }

        internal MethodInfo Method { get; private set; }
        internal Button Settings { get; private set; }
        internal ulong StableId { get; private set; }
        internal ParameterInfo[] Parameters { get; private set; }
        internal string DisplayName { get; private set; }

        private static string NiceName(string name)
        {
            if (string.IsNullOrEmpty(name))
                return string.Empty;
            StringBuilder result = new StringBuilder(name.Length + 8);
            for (int index = 0; index < name.Length; ++index)
            {
                char value = name[index];
                if (index != 0 && char.IsUpper(value) && (!char.IsUpper(name[index - 1]) || index + 1 < name.Length && char.IsLower(name[index + 1])))
                    result.Append(' ');
                result.Append(value);
            }
            return result.ToString();
        }
    }

    internal static class ScriptButtonMetadata
    {
        internal static ScriptButtonMethod[] Discover(Type type)
        {
            List<ScriptButtonMethod> methods = new List<ScriptButtonMethod>();
            HashSet<MethodInfo> virtualSlots = new HashSet<MethodInfo>();
            for (Type current = type; current != null && current != typeof(EntityBehaviour) && current != typeof(object); current = current.BaseType)
            {
                foreach (MethodInfo method in current.GetMethods(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Static |
                                                                  BindingFlags.Public | BindingFlags.NonPublic).OrderBy(value => value.MetadataToken))
                {
                    MethodInfo slot = method.IsVirtual ? method.GetBaseDefinition() : method;
                    if (!virtualSlots.Add(slot))
                        continue;
                    Button button = method.GetCustomAttributes(typeof(Button), true).FirstOrDefault() as Button;
                    if (button == null)
                        continue;
                    if (method.IsGenericMethodDefinition || method.ContainsGenericParameters || method.ReturnType.IsByRef || method.ReturnType.IsPointer)
                    {
                        Debug.Warn("Ignoring Button on unsupported method " + current.FullName + "." + method.Name + ".");
                        continue;
                    }
                    ParameterInfo[] parameters = method.GetParameters();
                    if (parameters.Any(parameter => parameter.ParameterType.IsByRef || parameter.ParameterType.IsPointer ||
                                                    !IsSupportedValue(parameter.ParameterType)))
                    {
                        Debug.Warn("Ignoring Button on " + current.FullName + "." + method.Name +
                                   " because one or more parameters cannot be represented by the inspector.");
                        continue;
                    }
                    methods.Add(new ScriptButtonMethod(method, button, MethodStableId(type, method)));
                }
            }
            return methods.ToArray();
        }

        private static string TypeIdentity(Type type)
        {
            return (type.Assembly.GetName().Name ?? string.Empty) + ":" + (type.FullName ?? type.Name);
        }

        internal static ulong MethodStableId(Type ownerType, MethodInfo method)
        {
            string signature = (ownerType.Assembly.GetName().Name ?? string.Empty) + ":" + ownerType.FullName + ":" + method.Name + "(" +
                               string.Join(",", method.GetParameters().Select(parameter => TypeIdentity(parameter.ParameterType))) + ")";
            return ManagedScriptCatalog.StableId(signature);
        }

        internal static bool IsSupportedValue(Type declaredType)
        {
            string kind = ScriptMetadata.ValueKind(declaredType);
            return ScriptMetadata.CanSerialize(declaredType) && kind != "Array" && kind != "List" && kind != "Dictionary" && kind != "Object" &&
                   kind != "Component";
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
            catalog.Add("DictionaryDisplays", WriteDictionaryDisplayRules(orderedTypes));
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
            Searchable typeSearch = type.GetCustomAttributes(typeof(Searchable), true).FirstOrDefault() as Searchable;
            if (typeSearch != null)
                result.Add("Searchable", WriteSearchable(typeSearch));
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
                WriteInspectorAttributes(field, member);
                fields.Add(field);
            }
            result.Add("Fields", fields);
            result.Add("Methods", WriteButtons(type));
            return result;
        }

        private static List<object> WriteButtons(Type type)
        {
            List<object> result = new List<object>();
            foreach (ScriptButtonMethod button in ScriptButtonMetadata.Discover(type))
            {
                Dictionary<string, object> method = new Dictionary<string, object>(StringComparer.Ordinal);
                method.Add("StableId", button.StableId);
                method.Add("Name", button.Method.Name);
                method.Add("IsStatic", button.Method.IsStatic);
                bool supportedResult = button.Method.ReturnType != typeof(void) && ScriptButtonMetadata.IsSupportedValue(button.Method.ReturnType);
                method.Add("ReturnKind", button.Method.ReturnType == typeof(void) ? "Null" : supportedResult ? ScriptMetadata.ValueKind(button.Method.ReturnType) : "String");
                method.Add("DeclaredReturnType", supportedResult ? DeclaredType(button.Method.ReturnType) : null);

                List<object> parameters = new List<object>();
                foreach (ParameterInfo parameter in button.Parameters)
                {
                    Dictionary<string, object> encoded = new Dictionary<string, object>(StringComparer.Ordinal);
                    encoded.Add("Name", parameter.Name ?? "Value");
                    encoded.Add("ValueKind", ScriptMetadata.ValueKind(parameter.ParameterType));
                    encoded.Add("DeclaredType", DeclaredType(parameter.ParameterType));
                    bool hasDefault = parameter.HasDefaultValue && parameter.DefaultValue != DBNull.Value && parameter.DefaultValue != Type.Missing;
                    encoded.Add("HasDefaultValue", hasDefault);
                    encoded.Add("DefaultValue", hasDefault ? ManagedStateCodec.EncodeInspectorValue(parameter.DefaultValue, parameter.ParameterType) : null);
                    parameters.Add(encoded);
                }
                method.Add("Parameters", parameters);
                method.Add("Button", WriteButton(button.Settings, button.DisplayName));
                result.Add(method);
            }
            return result;
        }

        private static Dictionary<string, object> WriteButton(Button button, string displayName)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("Name", displayName);
            result.Add("ButtonHeight", button.ButtonHeight);
            result.Add("ButtonAlignment", button.ButtonAlignment);
            result.Add("Stretch", button.Stretch);
            result.Add("Style", (uint)button.Style);
            result.Add("DisplayParameters", button.DisplayParameters);
            result.Add("Expanded", button.Expanded);
            result.Add("DrawResult", button.DrawResult);
            result.Add("DirtyOnClick", button.DirtyOnClick);
            result.Add("Icon", button.Icon ?? string.Empty);
            result.Add("IconAlignment", (uint)button.IconAlignment);
            return result;
        }

        internal static void WriteInspectorAttributes(Dictionary<string, object> target, ScriptMember member, object instance = null)
        {
            if (member.LabelText != null)
                target.Add("Label", member.LabelText);
            if (!string.IsNullOrEmpty(member.TooltipText))
                target.Add("Tooltip", member.TooltipText);
            if (member.SearchSettings != null)
                target.Add("Searchable", WriteSearchable(member.SearchSettings));
            if (member.ProgressBarSettings != null)
                target.Add("ProgressBar", WriteProgressBar(member.ProgressBarSettings));
            if (member.FilePathSettings != null)
                target.Add("FilePath", WriteFilePath(member.FilePathSettings));
            if (member.FolderPathSettings != null)
                target.Add("FolderPath", WriteFolderPath(member.FolderPathSettings));
            if (member.MultilineSettings != null)
                target.Add("Multiline", WriteMultiline(member.MultilineSettings));
            if (member.ColorUsageSettings != null)
                target.Add("ColorUsage", WriteColorUsage(member.ColorUsageSettings));
            if (member.EnumButtonsSettings != null)
                target.Add("EnumButtons", WriteEnumButtons(member.EnumButtonsSettings, member.EnumButtonsType));
            if (member.DictionaryDisplaySettings != null)
                target.Add("DictionaryDisplay", WriteDictionaryDisplay(member.DictionaryDisplaySettings));
            if (member.Conditions.Length != 0)
            {
                List<object> conditions = new List<object>();
                foreach (ScriptConditionDescriptor descriptor in member.Conditions)
                {
                    ConditionalAttribute attribute = descriptor.Attribute;
                    Dictionary<string, object> condition = new Dictionary<string, object>(StringComparer.Ordinal);
                    condition.Add("Effect", (uint)descriptor.Effect);
                    condition.Add("Condition", attribute.Condition);
                    condition.Add("Animate", attribute.AnimateVisibility);
                    condition.Add("HasValue", attribute.HasValue);
                    Type valueType = attribute.Value == null ? null : attribute.Value.GetType();
                    condition.Add("ValueKind", valueType == null ? "Null" : ScriptMetadata.ValueKind(valueType));
                    condition.Add("Value", valueType == null ? null : ManagedStateCodec.EncodeInspectorValue(attribute.Value, valueType));
                    if (instance != null)
                        condition.Add("Result", descriptor.Evaluate(instance));
                    conditions.Add(condition);
                }
                target.Add("Conditions", conditions);
            }
            if (member.ValueChangedActions.Length != 0)
            {
                List<object> actions = new List<object>();
                foreach (ScriptValueChangedDescriptor descriptor in member.ValueChangedActions)
                {
                    Dictionary<string, object> action = new Dictionary<string, object>(StringComparer.Ordinal);
                    action.Add("Action", descriptor.Attribute.Action);
                    action.Add("MethodId", descriptor.StableId);
                    action.Add("IncludeChildren", descriptor.Attribute.IncludeChildren);
                    action.Add("InvokeOnInitialize", descriptor.Attribute.InvokeOnInitialize);
                    action.Add("InvokeOnUndoRedo", descriptor.Attribute.InvokeOnUndoRedo);
                    action.Add("PassValue", descriptor.PassValue);
                    actions.Add(action);
                }
                target.Add("OnValueChanged", actions);
            }
        }

        private static Dictionary<string, object> WriteSearchable(Searchable searchable)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("FilterOptions", (uint)searchable.FilterOptions);
            result.Add("FuzzySearch", searchable.FuzzySearch);
            result.Add("Recursive", searchable.Recursive);
            return result;
        }

        private static Dictionary<string, object> WriteProgressBar(ProgressBar progressBar)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("Min", progressBar.Min);
            result.Add("Max", progressBar.Max);
            result.Add("MinGetter", progressBar.MinGetter ?? string.Empty);
            result.Add("MaxGetter", progressBar.MaxGetter ?? string.Empty);
            result.Add("R", progressBar.R);
            result.Add("G", progressBar.G);
            result.Add("B", progressBar.B);
            result.Add("Height", progressBar.Height);
            result.Add("Segmented", progressBar.Segmented);
            result.Add("DrawValueLabel", progressBar.DrawValueLabel);
            result.Add("ValueLabelAlignment", (uint)progressBar.ValueLabelAlignment);
            result.Add("ColorGetter", progressBar.ColorGetter ?? string.Empty);
            result.Add("BackgroundColorGetter", progressBar.BackgroundColorGetter ?? string.Empty);
            result.Add("CustomValueStringGetter", progressBar.CustomValueStringGetter ?? string.Empty);
            return result;
        }

        private static Dictionary<string, object> WriteFilePath(FilePath filePath)
        {
            Dictionary<string, object> result = WritePath(filePath.AbsolutePath, filePath.ParentFolder, filePath.RequireExistingPath,
                                                          filePath.UseBackslashes);
            result.Add("Extensions", filePath.Extensions ?? string.Empty);
            result.Add("IncludeFileExtension", filePath.IncludeFileExtension);
            return result;
        }

        private static Dictionary<string, object> WriteFolderPath(FolderPath folderPath)
        {
            return WritePath(folderPath.AbsolutePath, folderPath.ParentFolder, folderPath.RequireExistingPath, folderPath.UseBackslashes);
        }

        private static Dictionary<string, object> WritePath(bool absolutePath, string parentFolder, bool requireExistingPath,
                                                            bool useBackslashes)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("AbsolutePath", absolutePath);
            result.Add("ParentFolder", parentFolder ?? string.Empty);
            result.Add("RequireExistingPath", requireExistingPath);
            result.Add("UseBackslashes", useBackslashes);
            return result;
        }

        private static Dictionary<string, object> WriteMultiline(Multiline multiline)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("Lines", multiline.Lines);
            return result;
        }

        internal static Dictionary<string, object> WriteColorUsage(ColorUsage colorUsage)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("ShowAlpha", colorUsage.showAlpha);
            result.Add("Hdr", colorUsage.hdr);
            return result;
        }

        private static Dictionary<string, object> WriteEnumButtons(EnumButtons enumButtons, Type enumType)
        {
            Type underlyingType = Enum.GetUnderlyingType(enumType);
            bool unsigned = underlyingType == typeof(byte) || underlyingType == typeof(ushort) ||
                            underlyingType == typeof(uint) || underlyingType == typeof(ulong);
            List<object> options = new List<object>();
            foreach (FieldInfo enumField in enumType.GetFields(BindingFlags.Public | BindingFlags.Static)
                                                    .OrderBy(field => field.MetadataToken))
            {
                ObsoleteAttribute obsolete = enumField.GetCustomAttributes(typeof(ObsoleteAttribute), false)
                                                        .FirstOrDefault() as ObsoleteAttribute;
                if (obsolete != null && (!enumButtons.includeObsolete || obsolete.IsError))
                    continue;

                object rawValue = enumField.GetRawConstantValue();
                ulong value = unsigned ? Convert.ToUInt64(rawValue) : unchecked((ulong)Convert.ToInt64(rawValue));
                Dictionary<string, object> option = new Dictionary<string, object>(StringComparer.Ordinal);
                option.Add("Name", enumField.Name);
                option.Add("Value", value);
                options.Add(option);
            }

            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("IsFlags", Attribute.IsDefined(enumType, typeof(FlagsAttribute), false));
            result.Add("IsUnsigned", unsigned);
            result.Add("IncludeObsolete", enumButtons.includeObsolete);
            result.Add("Options", options);
            return result;
        }

        internal static Dictionary<string, object> WriteDictionaryDisplay(DictionaryDisplay display)
        {
            Dictionary<string, object> result = new Dictionary<string, object>(StringComparer.Ordinal);
            result.Add("Layout", (uint)display.layout);
            result.Add("KeyLabel", display.keyLabel ?? string.Empty);
            result.Add("ValueLabel", display.valueLabel ?? string.Empty);
            result.Add("KeyColumnFraction", display.keyColumnFraction);
            return result;
        }

        private static List<object> WriteDictionaryDisplayRules(Type[] scriptTypes)
        {
            List<object> result = new List<object>();
            foreach (Assembly assembly in scriptTypes.Select(type => type.Assembly).Distinct().OrderBy(value => value.FullName, StringComparer.Ordinal))
            {
                foreach (DictionaryDisplayForType rule in assembly.GetCustomAttributes(typeof(DictionaryDisplayForType), false)
                                                                      .Cast<DictionaryDisplayForType>())
                {
                    Type targetType = rule.targetType;
                    bool validDictionary = targetType != null && targetType.IsGenericType && !targetType.ContainsGenericParameters &&
                                           targetType.GetGenericTypeDefinition() == typeof(Dictionary<,>);
                    if (!validDictionary || !UsesAssemblyType(targetType, assembly))
                    {
                        Debug.Warn("Ignoring DictionaryDisplayForType for " + (targetType == null ? "null" : targetType.ToString()) +
                                   " because it is not a closed dictionary containing a type from " + assembly.GetName().Name + ".");
                        continue;
                    }

                    Dictionary<string, object> encoded = WriteDictionaryDisplay(rule);
                    encoded.Add("TargetType", Identity(targetType));
                    result.Add(encoded);
                }
            }
            return result;
        }

        private static bool UsesAssemblyType(Type type, Assembly assembly)
        {
            if (type.Assembly == assembly)
                return true;
            if (type.HasElementType)
                return UsesAssemblyType(type.GetElementType(), assembly);
            if (type.IsGenericType)
                return type.GetGenericArguments().Any(argument => UsesAssemblyType(argument, assembly));
            return false;
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

        internal static ulong StableId(string text)
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
            return kind == "Object" || kind == "Component" || kind == "Dictionary" ? Identity(type) : null;
        }

        private static bool IsGeneric(Type type, Type definition)
        {
            return type.IsGenericType && type.GetGenericTypeDefinition() == definition;
        }
    }
}
