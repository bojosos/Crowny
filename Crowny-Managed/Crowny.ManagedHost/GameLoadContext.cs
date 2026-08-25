using System.Reflection;
using System.Runtime.Loader;

namespace Crowny.ManagedHost;

internal sealed class GameLoadContext : AssemblyLoadContext
{
    private readonly AssemblyDependencyResolver _resolver;

    internal GameLoadContext(string mainAssemblyPath) : base($"CrownyGame:{Path.GetFileNameWithoutExtension(mainAssemblyPath)}", true)
    {
        _resolver = new AssemblyDependencyResolver(mainAssemblyPath);
    }

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        if (assemblyName.Name is "CrownySharp" or "Crowny.ManagedHost")
            return null;
        string? path = _resolver.ResolveAssemblyToPath(assemblyName);
        return path is null ? null : LoadFromAssemblyPath(path);
    }

    protected override nint LoadUnmanagedDll(string unmanagedDllName)
    {
        string? path = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
        return path is null ? nint.Zero : LoadUnmanagedDllFromPath(path);
    }
}
