using System;

namespace Crowny
{
    /// <summary>
    /// Folderpath can be used to show a folder selection control in the inspector for strings.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class FolderPath : Attribute
    {
#pragma warning disable 0414
        FileDialogType type;
#pragma warning restore 0414

        public FolderPath(FileDialogType type = FileDialogType.OpenFolder)
        {
            this.type = type;
        }
    }
}