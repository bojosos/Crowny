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
        /// <param name="extension">A comma-separated list of extension filters. Leading periods are optional.</param>
        /// <returns>Returns the selected path.</returns>
        public static string OpenFileDialog(string title = "Open File", string directory = "", string extension = "")
        {
            return ManagedRuntimeContext.FileDialogOpenFile(title, directory, extension);
        }

        /// <summary>
        /// Open a folder selection dialog.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <returns>Returns the selected path.</returns>
        public static string OpenFolderDialog(string title = "Open Folder", string directory = "")
        {
            return ManagedRuntimeContext.FileDialogOpenFolder(title, directory);
        }

        /// <summary>
        /// Open a file saving dialog.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <param name="defaultName">The default name of the file.</param>
        /// <param name="extension">A comma-separated list of extension filters. Leading periods are optional.</param>
        /// <returns>Returns the selected path.</returns>
        public static string SaveFileDialog(string title = "Save File", string directory = "", string defaultName = "", string extension = "")
        {
            return ManagedRuntimeContext.FileDialogSaveFile(title, directory, defaultName, extension);
        }

        /// <summary>
        /// Open a folder selection dialog with a default folder name.
        /// </summary>
        /// <param name="title">The title of the dialog.</param>
        /// <param name="directory">The default directory the dialog opens in.</param>
        /// <param name="defaultName">The default name of the folder.</param>
        /// <returns>Returns the selected path.</returns>
        public static string SaveFolderDialog(string title = "Save Folder", string directory = "", string defaultName = "")
        {
            return ManagedRuntimeContext.FileDialogSaveFolder(title, directory, defaultName);
        }

    }
}
