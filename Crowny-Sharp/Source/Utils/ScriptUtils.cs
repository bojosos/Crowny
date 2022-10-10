using System;
using System.Threading;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Diagnostics;

namespace Crowny
{

    public static class ScriptUtils
    {
        public static string[] GetEnumNames(Type type)
        {
            return Enum.GetNames(type);
        }
    }

    public static class ScriptCompiler
    {
        public enum ScriptAssemblyType
        {
            Game, Editor
        }

        private static Process process;
        private static Thread readErrorsThread;

        public static void Compile(ScriptAssemblyType type, bool debug, string outputDirectory, string projectPath, string[] libDirs, string[] references)
        {
            string[] files = Directory.GetFiles(projectPath, "*.cs", SearchOption.AllDirectories);
            ProcessStartInfo psi = new ProcessStartInfo();
            StringBuilder argBuilder = new StringBuilder();

            argBuilder.Append("-noconfig");

            if (debug)
                argBuilder.Append(" -debug+ -o-");
            else
                argBuilder.Append(" -debug- -o+");

            argBuilder.Append(" -target:library -out:" + "\"" + Path.Combine(outputDirectory, "GameAssembly.dll") + "\"");

            if (libDirs != null && libDirs.Length > 0)
            {
                argBuilder.Append(" -lib:\"");
                for (int i = 0; i < libDirs.Length - 1; i++)
                    argBuilder.Append(libDirs[i] + ",");
                argBuilder.Append(libDirs[libDirs.Length - 1] + "\"");
            }

            if (references != null && references.Length > 0)
            {
                argBuilder.Append(" -r:");
                for (int i = 0; i < references.Length - 1; i++)
                    argBuilder.Append(references[i] + ",");
                argBuilder.Append(references[references.Length - 1]);
                argBuilder.Append(",System");
            }

            foreach (string file in files)
                argBuilder.Append(" \"" + file + "\"");

            if (File.Exists(outputDirectory))
                File.Delete(outputDirectory);
            if (!Directory.Exists(outputDirectory))
                Directory.CreateDirectory(outputDirectory);

            psi.Arguments = argBuilder.ToString();
            Debug.Log(psi.Arguments);
            psi.CreateNoWindow = true;
            psi.FileName = "C:\\Program Files\\Mono\\bin\\mcs.bat";
            psi.RedirectStandardError = true;
            psi.RedirectStandardOutput = false;
            psi.UseShellExecute = false;
            psi.WorkingDirectory = projectPath;

            process = new Process();
            process.StartInfo = psi;
            process.Start();

            readErrorsThread = new Thread(ReadErrors);
            readErrorsThread.Start();
            process.WaitForExit();
            // Debug.Log("Compiled...");
        }

        private static void ReadErrors()
        {
            while (true)
            {
                if (process == null || process.HasExited)
                    return;

                string read = process.StandardError.ReadLine();
                if (string.IsNullOrEmpty(read))
                    continue;

                if (read.Contains(": warning"))
                    Debug.Warn(read);
                else if (read.Contains(": error"))
                    Debug.Error(read);
            }
        }
    }
}