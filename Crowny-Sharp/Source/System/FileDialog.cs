using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
  
    public enum FileDialogType
    {
        // A browser dialog for selecting a single file.
        OpenFile = 0,
        // A browser dialog for selecting a folder.
        OpenFolder = 1,
        // A browser dialog for saving a file.
        SaveFile = 2,
        // A browser dialog for multiselect.
        Multiselect = 3
    }

    /// <summary>
    /// Class used for opening file browser dialogs.
    /// </summary>
    public static class FileDialog
    {
        /// <summary>
        /// Open a file browser dialog.
        /// </summary>
        /// <param name="type">Type of dialog.</param>
        public static void Open(FileDialogType type)
        {
            Internal_OpenDialog(type);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_OpenDialog(FileDialogType type);
    }
}