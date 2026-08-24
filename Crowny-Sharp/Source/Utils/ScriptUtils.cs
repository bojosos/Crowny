using System;
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

        public static int[] GetEnumValuesInt32(Type type)
        {
            Array values = Enum.GetValues(type);
            int[] result = new int[values.Length];
            Type underlyingType = Enum.GetUnderlyingType(type);
            bool unsigned = underlyingType == typeof(byte) || underlyingType == typeof(ushort) ||
                            underlyingType == typeof(uint) || underlyingType == typeof(ulong);

            for (int i = 0; i < values.Length; i++)
            {
                object value = values.GetValue(i);
                result[i] = unsigned ? unchecked((int)Convert.ToUInt64(value)) : unchecked((int)Convert.ToInt64(value));
            }
            return result;
        }
    }

    public static class ScriptCompiler
    {
        public enum ScriptAssemblyType
        {
            Game, Editor
        }

        public static bool Compile(ScriptAssemblyType type, bool debug, string outputDirectory, string projectPath, string[] libDirs,
                                   string[] references, string compilerPath)
        {
            if (!Directory.Exists(projectPath))
            {
                Debug.Error("Script source directory does not exist: " + projectPath);
                return false;
            }
            if (string.IsNullOrEmpty(compilerPath) || !File.Exists(compilerPath))
            {
                Debug.Error("Mono C# compiler does not exist: " + compilerPath);
                return false;
            }

            string[] files = Directory.GetFiles(projectPath, "*.cs", SearchOption.AllDirectories);
            Array.Sort(files, StringComparer.Ordinal);
            StringBuilder argBuilder = new StringBuilder();

            argBuilder.Append("-noconfig");

            if (debug)
                argBuilder.Append(" -debug:portable -optimize-");
            else
                argBuilder.Append(" -debug- -optimize+");

            argBuilder.Append(" -target:library -out:" + "\"" + Path.Combine(outputDirectory, "GameAssembly.dll") + "\"");

            if (libDirs != null && libDirs.Length > 0)
            {
                argBuilder.Append(" -lib:\"");
                for (int i = 0; i < libDirs.Length - 1; i++)
                    argBuilder.Append(libDirs[i] + ";");
                argBuilder.Append(libDirs[libDirs.Length - 1] + "\"");
            }

            if (references != null && references.Length > 0)
            {
                foreach (string reference in references)
                    argBuilder.Append(" -r:\"").Append(reference).Append("\"");
            }
            argBuilder.Append(" -r:System.dll");

            foreach (string file in files)
                argBuilder.Append(" \"" + file + "\"");

            if (!Directory.Exists(outputDirectory))
                Directory.CreateDirectory(outputDirectory);

            ProcessStartInfo psi = new ProcessStartInfo();
            string compilerExtension = Path.GetExtension(compilerPath);
            bool isWindows = Path.DirectorySeparatorChar == '\\';
            if (isWindows && (string.Equals(compilerExtension, ".bat", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(compilerExtension, ".cmd", StringComparison.OrdinalIgnoreCase))
               )
            {
                psi.FileName = Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe";
                psi.Arguments = "/d /s /c \"\"" + compilerPath + "\" " + argBuilder + "\"";
            }
            else
            {
                psi.FileName = compilerPath;
                psi.Arguments = argBuilder.ToString();
            }
            psi.CreateNoWindow = true;
            psi.RedirectStandardError = true;
            psi.RedirectStandardOutput = true;
            psi.UseShellExecute = false;
            psi.WorkingDirectory = projectPath;

            try
            {
                using (Process process = new Process())
                {
                    process.StartInfo = psi;
                    process.OutputDataReceived += (sender, args) => LogCompilerLine(args.Data, false);
                    process.ErrorDataReceived += (sender, args) => LogCompilerLine(args.Data, true);
                    if (!process.Start())
                        return false;
                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();
                    process.WaitForExit();
                    process.WaitForExit();

                    string outputAssembly = Path.Combine(outputDirectory, "GameAssembly.dll");
                    if (process.ExitCode != 0 || !File.Exists(outputAssembly))
                    {
                        Debug.Error("C# compiler exited with code " + process.ExitCode + ".");
                        return false;
                    }
                }
            }
            catch (Exception exception)
            {
                Debug.Error("Could not run the C# compiler: " + exception.Message);
                return false;
            }
            return true;
        }

        private static void LogCompilerLine(string line, bool standardError)
        {
            if (string.IsNullOrWhiteSpace(line))
                return;
            if (line.Contains(": error") || standardError)
                Debug.Error(line);
            else if (line.Contains(": warning"))
                Debug.Warn(line);
            else
                Debug.Log(line);
        }
    }
}
