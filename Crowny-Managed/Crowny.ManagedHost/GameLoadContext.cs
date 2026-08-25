using System.Reflection;
using System.Runtime.Loader;

namespace Crowny.ManagedHost;

internal sealed class GameLoadContext : AssemblyLoadContext
{
    private readonly AssemblyDependencyResolver _resolver;
    private readonly AssemblyLoadContext _sharedContext;

    internal GameLoadContext(string mainAssemblyPath) : base($"CrownyGame:{Path.GetFileNameWithoutExtension(mainAssemblyPath)}", true)
    {
        _resolver = new AssemblyDependencyResolver(mainAssemblyPath);
        _sharedContext = GetLoadContext(typeof(GameLoadContext).Assembly) ?? Default;
    }

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        if (assemblyName.Name == "Crowny.ManagedHost")
            return typeof(GameLoadContext).Assembly;
        if (assemblyName.Name == "CrownySharp")
        {
            Assembly? shared = _sharedContext.Assemblies.FirstOrDefault(
                loaded => AssemblyName.ReferenceMatchesDefinition(loaded.GetName(), assemblyName));
            if (shared is not null)
                return shared;

            string? sharedPath = _resolver.ResolveAssemblyToPath(assemblyName);
            return sharedPath is null ? null : _sharedContext.LoadFromAssemblyPath(sharedPath);
        }
        string? path = _resolver.ResolveAssemblyToPath(assemblyName);
        return path is null ? null : LoadFromAssemblyPath(path);
    }

    protected override nint LoadUnmanagedDll(string unmanagedDllName)
    {
        string? path = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
        return path is null ? nint.Zero : LoadUnmanagedDllFromPath(path);
    }
}
