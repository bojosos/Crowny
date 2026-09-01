using System;

namespace Crowny
{
    /// <summary>
    /// Draws a string field or property with a file picker in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public class FilePath : Attribute
    {
        /// <summary>Gets or sets whether the inspector stores an absolute path.</summary>
        public bool AbsolutePath { get; set; }

        /// <summary>Gets or sets the comma-separated allowed extensions. Prefix with '$' to read them from a sibling string member.</summary>
        public string Extensions { get; set; }

        /// <summary>Gets or sets whether the stored path includes its file extension.</summary>
        public bool IncludeFileExtension { get; set; } = true;

        /// <summary>
        /// Gets or sets the directory paths are relative to. Prefix with '$' to read it from a sibling string member.
        /// </summary>
        public string ParentFolder { get; set; }

        /// <summary>Gets or sets whether the inspector reports paths that do not identify an existing file.</summary>
        public bool RequireExistingPath { get; set; }

        /// <summary>Compatibility alias for <see cref="RequireExistingPath"/>.</summary>
        [Obsolete("Use RequireExistingPath instead.")]
        public bool RequireValidPath
        {
            get { return RequireExistingPath; }
            set { RequireExistingPath = value; }
        }

        /// <summary>Gets or sets whether stored paths use backslashes instead of forward slashes.</summary>
        public bool UseBackslashes { get; set; }
    }

    /// <summary>
    /// Compatibility spelling for <see cref="FilePath"/>.
    /// </summary>
    [Obsolete("Use FilePath instead.")]
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public sealed class Filepath : FilePath
    {
        /// <summary>Creates a file-path picker. The dialog type is retained for source compatibility.</summary>
        public Filepath(FileDialogType type = FileDialogType.OpenFile)
        {
            DialogType = type;
        }

        internal FileDialogType DialogType { get; private set; }
    }
}
