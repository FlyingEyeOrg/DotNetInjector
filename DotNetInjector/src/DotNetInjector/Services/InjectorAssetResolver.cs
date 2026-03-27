using System.IO;
using DotNetInjector.Models;

namespace DotNetInjector.Services;

internal static class InjectorAssetResolver
{
    public static (string ToolPath, string PayloadPath) Resolve(InjectionRuntimeKind runtime, string architecture)
    {
        var baseDirectory = AppContext.BaseDirectory;
        var normalizedArchitecture = architecture.Equals("x64", StringComparison.OrdinalIgnoreCase) ? "x64" : "x86";
        var toolDirectory = Path.Combine(baseDirectory, "Tools", normalizedArchitecture);

        return (
            ResolveToolPath(toolDirectory),
            Path.Combine(toolDirectory, "ManagedInjectionLibrary.dll"));
    }

    private static string ResolveToolPath(string toolDirectory)
    {
        foreach (var candidate in new[] { "WinInjector.exe", "wininjector.exe" })
        {
            var fullPath = Path.Combine(toolDirectory, candidate);
            if (File.Exists(fullPath))
            {
                return fullPath;
            }
        }

        return Path.Combine(toolDirectory, "WinInjector.exe");
    }
}