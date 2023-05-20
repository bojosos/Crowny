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
        /// Open a file selection dialog.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <param name="extension">A comma separated list of strings to be used for extension filters in the dialog. Do not used precede with a period.</param>
        /// <returns>Returns the selected path.</returns>
        public static string OpenFileDialog(string title, string directory, string extension)
        {
            return Internal_OpenFileDialog(title, directory, extension);
        }

        /// <summary>
        /// Open a folder selection dialog.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <returns>Returns the selected path.</returns>
        public static string OpenFolderDialog(string title, string directory)
        {
            return Internal_OpenFolderDialog(title, directory);
        }

        /// <summary>
        /// Open a file saving dialog.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <param name="defaultName">The default name of the file.</param>
        /// <param name="extension">A comma separated list of strings to be used for extension filters in the dialog. Do not used precede with a period.</param>
        /// <returns>Returns the selected path.</returns>
        public static string SaveFileDialog(string title, string directory, string defaultName, string extension)
        {
            return Internal_SaveFileDialog(title, directory, defaultName, extension);
        }

        /// <summary>
        /// Open a file dialog.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <param name="defaultName">The default name of the folder.</param>
        /// <returns>Returns the selected path.</returns>
        public static string SaveFolderDialog(string title, string directory, string defaultName)
        {
            return Internal_SaveFolderDialog(title, directory, defaultName);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_OpenFileDialog(string title, string directory, string extension);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_OpenFolderDialog(string title, string directory);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_SaveFileDialog(string title, string directory, string defaultName, string extension);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_SaveFolderDialog(string title, string directory, string defualtName);
    }
}