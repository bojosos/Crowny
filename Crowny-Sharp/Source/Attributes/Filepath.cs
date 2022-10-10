using System;

namespace Crowny
{
    /// <summary>
    /// Filepath can be used to show a file selection control in the inspector for strings.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class Filepath : Attribute
    {
#pragma warning disable 0414
        FileDialogType type;
#pragma warning restore 0414

        public Filepath(FileDialogType type = FileDialogType.OpenFile)
        {
            this.type = type;
        }
    }
}