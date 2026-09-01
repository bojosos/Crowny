using System;

namespace Crowny
{
    /// <summary>
    /// Draws a string field or property with a folder picker in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public sealed class FolderPath : Attribute
    {
        /// <summary>Creates a folder-path picker.</summary>
        public FolderPath() {}

        /// <summary>Creates a folder-path picker. The dialog type is retained for source compatibility.</summary>
        /// <param name="type">Ignored. Folder paths always use a folder picker.</param>
        [Obsolete("FolderPath always opens a folder picker.")]
        public FolderPath(FileDialogType type) {}

        /// <summary>Gets or sets whether the inspector stores an absolute path.</summary>
        public bool AbsolutePath { get; set; }

        /// <summary>
        /// Gets or sets the directory paths are relative to. Prefix with '$' to read it from a sibling string member.
        /// </summary>
        public string ParentFolder { get; set; }

        /// <summary>Gets or sets whether the inspector reports paths that do not identify an existing directory.</summary>
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
}
