using System;

namespace Crowny
{
    /// <summary>Preset heights for methods drawn as inspector buttons.</summary>
    public enum ButtonSizes
    {
        Small = 20,
        Medium = 30,
        Large = 40,
        Gigantic = 60
    }

    /// <summary>Controls how an inspector button and its parameters are grouped.</summary>
    public enum ButtonStyle
    {
        Box,
        CompactBox,
        FoldoutButton
    }

    /// <summary>Controls which side of the caption contains a button icon.</summary>
    public enum ButtonIconAlignment
    {
        Left,
        Right
    }

    /// <summary>Draws a method as an invokable button in the component inspector.</summary>
    [AttributeUsage(AttributeTargets.Method, Inherited = true, AllowMultiple = false)]
    public sealed class Button : Attribute
    {
        public Button() { }

        public Button(string name) { Name = name; }

        public Button(ButtonSizes size) { ButtonHeight = (int)size; }

        public Button(int buttonHeight) { ButtonHeight = buttonHeight; }

        public Button(ButtonStyle style) { Style = style; }

        public Button(ButtonSizes size, ButtonStyle style) : this(size) { Style = style; }

        public Button(int buttonHeight, ButtonStyle style) : this(buttonHeight) { Style = style; }

        public Button(string name, ButtonSizes size) : this(name) { ButtonHeight = (int)size; }

        public Button(string name, int buttonHeight) : this(name) { ButtonHeight = buttonHeight; }

        public Button(string name, ButtonStyle style) : this(name) { Style = style; }

        public Button(string name, ButtonSizes size, ButtonStyle style) : this(name, size) { Style = style; }

        public Button(string name, int buttonHeight, ButtonStyle style) : this(name, buttonHeight) { Style = style; }

        /// <summary>Gets or sets the caption. An empty caption is generated from the method name.</summary>
        public string Name { get; set; }

        /// <summary>Gets or sets the button height in pixels. Zero uses the current UI default.</summary>
        public int ButtonHeight { get; set; }

        /// <summary>Gets or sets the horizontal caption alignment from zero (left) to one (right).</summary>
        public float ButtonAlignment { get; set; } = 0.5f;

        /// <summary>Gets or sets whether the button fills the available inspector width.</summary>
        public bool Stretch { get; set; } = true;

        /// <summary>Gets or sets how the button and parameter controls are grouped.</summary>
        public ButtonStyle Style { get; set; } = ButtonStyle.CompactBox;

        /// <summary>Gets or sets whether method parameters are displayed.</summary>
        public bool DisplayParameters { get; set; } = true;

        /// <summary>Gets or sets the initial expanded state for parameter/result content.</summary>
        public bool Expanded { get; set; }

        /// <summary>Gets or sets whether a non-void return value is shown below the button.</summary>
        public bool DrawResult { get; set; } = true;

        /// <summary>Gets or sets whether a click records and persists changes made by the method.</summary>
        public bool DirtyOnClick { get; set; } = true;

        /// <summary>Gets or sets optional icon or glyph text drawn next to the caption.</summary>
        public string Icon { get; set; }

        /// <summary>Gets or sets which side of the caption contains the icon.</summary>
        public ButtonIconAlignment IconAlignment { get; set; } = ButtonIconAlignment.Left;
    }
}
