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

        private enum CompilerMessageType
        {
            Warning, Error
        }

        private struct CompilerMessage
        {
            public CompilerMessageType type;
            public string message;
            public string file;
            public int line, column;
        }

        public enum ScriptAssemblyType
        {
            Game, Editor
        }

        private static Process process;
        private static Thread readErrorsThread;

        private static List<CompilerMessage> errors = new List<CompilerMessage>();
        private static List<CompilerMessage> warnings = new List<CompilerMessage>();

        private static Regex compileErrorRegex = new Regex(@"\s*(?<file>.*)\(\s*(?<line>\d+)\s*,\s*(?<column>\d+)\s*\)\s*:\s*(?<type>warning|error)\s+(.*):\s*(?<message>.*)");
        private static Regex compilerErrorRegex = new Regex(@"\s*error[^:]*:\s*(?<message>.*)");

        public static void Compile(ScriptAssemblyType type, bool debug, string outputDirectory, string projectPath, string[] libDirs, string[] references)
        {
            errors.Clear();
            warnings.Clear();
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
            }

            foreach (string file in files)
                argBuilder.Append(" \"" + file + "\"");

            if (File.Exists(outputDirectory))
                File.Delete(outputDirectory);
            if (!Directory.Exists(outputDirectory))
                Directory.CreateDirectory(outputDirectory);

            psi.Arguments = argBuilder.ToString();
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
        }

        private static void ReadErrors()
        {
            while (true)
            {
                if (process.HasExited)
                    Debug.Log("Compiled");
                if (process == null || process.HasExited)
                    return;

                string read = process.StandardError.ReadLine();
                if (string.IsNullOrEmpty(read))
                    continue;

                CompilerMessage message;
                if (TryParseMessage(read, out message))
                {
                    if (message.type == CompilerMessageType.Warning)
                    {
                        Debug.LogWarning(message.message);
                        /*lock (warnings)
                            warnings.Add(message);*/
                    }
                    else if (message.type == CompilerMessageType.Error)
                    {
                        Debug.LogError(message);
                        /*lock (errors)
                            errors.Add(message);*/
                    }
                }
            }
        }

        private static bool TryParseMessage(string messageText, out CompilerMessage message)
        {
            message = new CompilerMessage();

            Match matchCompile = compileErrorRegex.Match(messageText);
            if (matchCompile.Success)
            {
                message.file = matchCompile.Groups["file"].Value;
                int val = 0;
                int.TryParse(matchCompile.Groups["line"].Value, out val);
                message.line = val;
                int.TryParse(matchCompile.Groups["column"].Value, out val);
                message.column = val;
                message.type = matchCompile.Groups["type"].Value == "error" ? CompilerMessageType.Error : CompilerMessageType.Warning;
                message.message = matchCompile.Groups["message"].Value;

                return true;
            }

            Match matchCompiler = compilerErrorRegex.Match(messageText);
            if (matchCompiler.Success)
            {
                message.file = "";
                message.line = 0;
                message.column = 0;
                message.type = CompilerMessageType.Error;
                message.message = matchCompiler.Groups["message"].Value;

                return true;
            }

            return false;
        }
    }
}