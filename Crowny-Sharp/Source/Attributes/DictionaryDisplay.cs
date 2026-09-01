using System;

namespace Crowny
{
    public enum DictionaryLayout
    {
        TwoColumns,
        OneColumnWithValueFoldout,
        OneColumnWithValueVisible
    }

    /// <summary>
    /// Controls how a dictionary is drawn in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = false)]
    public class DictionaryDisplay : Attribute
    {
        private float keyColumnFractionValue = 0.5f;

        public DictionaryLayout layout { get; set; } = DictionaryLayout.TwoColumns;
        public string keyLabel { get; set; } = string.Empty;
        public string valueLabel { get; set; } = string.Empty;

        public float keyColumnFraction
        {
            get { return keyColumnFractionValue; }
            set
            {
                if (float.IsNaN(value))
                    keyColumnFractionValue = 0.5f;
                else
                    keyColumnFractionValue = Math.Max(0.01f, Math.Min(0.99f, value));
            }
        }

        public DictionaryLayout Layout
        {
            get { return layout; }
            set { layout = value; }
        }

        public string KeyLabel
        {
            get { return keyLabel; }
            set { keyLabel = value; }
        }

        public string ValueLabel
        {
            get { return valueLabel; }
            set { valueLabel = value; }
        }

        public float KeyColumnFraction
        {
            get { return keyColumnFraction; }
            set { keyColumnFraction = value; }
        }
    }

    /// <summary>
    /// Applies dictionary display settings to every dictionary of an exact constructed type in the assembly.
    /// </summary>
    [AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
    public sealed class DictionaryDisplayForType : DictionaryDisplay
    {
        public DictionaryDisplayForType(Type targetType)
        {
            this.targetType = targetType;
        }

        public Type targetType { get; private set; }

        public Type TargetType
        {
            get { return targetType; }
        }
    }
}
