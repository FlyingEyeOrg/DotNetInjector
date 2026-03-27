using DotNetInjector.Models;
using DotNetInjector.Services;

namespace DotNetInjector.Tests;

internal static class InjectorAssetResolverTests
{
    public static void Run()
    {
        Resolve_AlwaysUsesX64Injector_AndMatchesPayloadArchitecture("x86", "x64", "x86");
        Resolve_AlwaysUsesX64Injector_AndMatchesPayloadArchitecture("x64", "x64", "x64");
    }

    private static void Resolve_AlwaysUsesX64Injector_AndMatchesPayloadArchitecture(
        string targetArchitecture,
        string expectedToolArchitecture,
        string expectedPayloadArchitecture)
    {
        var base_directory = Path.Combine(Path.GetTempPath(), "DotNetInjector-ResolverTests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(base_directory, "Tools", "x64"));
        Directory.CreateDirectory(Path.Combine(base_directory, "Tools", "x86"));

        var tool_path = Path.Combine(base_directory, "Tools", "x64", "WinInjector.exe");
        var x64_payload_path = Path.Combine(base_directory, "Tools", "x64", "ManagedInjectionLibrary.dll");
        var x86_payload_path = Path.Combine(base_directory, "Tools", "x86", "ManagedInjectionLibrary.dll");
        File.WriteAllText(tool_path, string.Empty);
        File.WriteAllText(x64_payload_path, string.Empty);
        File.WriteAllText(x86_payload_path, string.Empty);

        var (resolved_tool_path, resolved_payload_path) = InjectorAssetResolver.Resolve(
            InjectionRuntimeKind.DotNet,
            targetArchitecture,
            base_directory);

        AssertEx.Contains($"Tools{Path.DirectorySeparatorChar}{expectedToolArchitecture}{Path.DirectorySeparatorChar}WinInjector.exe", resolved_tool_path, "resolved tool path mismatch");
        AssertEx.Contains($"Tools{Path.DirectorySeparatorChar}{expectedPayloadArchitecture}{Path.DirectorySeparatorChar}ManagedInjectionLibrary.dll", resolved_payload_path, "resolved payload path mismatch");
    }
}