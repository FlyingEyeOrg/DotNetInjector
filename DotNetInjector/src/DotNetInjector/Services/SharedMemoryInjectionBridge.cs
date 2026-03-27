using DotNetInjector.Models;
using DotNetInjector.Utils;

namespace DotNetInjector.Services;

internal sealed class SharedMemoryInjectionBridge : IDisposable
{
    private readonly SharedMemWriter framework_version_mem_ = new();
    private readonly SharedMemWriter assembly_path_mem_ = new();
    private readonly SharedMemWriter entry_class_mem_ = new();
    private readonly SharedMemWriter entry_method_mem_ = new();
    private readonly SharedMemWriter entry_argument_mem_ = new();

    public SharedMemoryInjectionBridge()
    {
        framework_version_mem_.Create("FrameworkVersion", 128 * 2);
        assembly_path_mem_.Create("AssemblyFile", 512 * 2);
        entry_class_mem_.Create("EntryClass", 256 * 2);
        entry_method_mem_.Create("EntryMethod", 128 * 2);
        entry_argument_mem_.Create("EntryArgument", 512 * 2);
    }

    public void Publish(ManagedInjectionRequest request)
    {
        framework_version_mem_.Write(GetFrameworkToken(request));
        assembly_path_mem_.Write(request.AssemblyPath);
        entry_class_mem_.Write(request.EntryClass);
        entry_method_mem_.Write(request.EntryMethod);
        entry_argument_mem_.Write(request.EntryArgument);
    }

    public void Dispose()
    {
        framework_version_mem_.Dispose();
        assembly_path_mem_.Dispose();
        entry_class_mem_.Dispose();
        entry_method_mem_.Dispose();
        entry_argument_mem_.Dispose();
    }

    private static string GetFrameworkToken(ManagedInjectionRequest request)
    {
        return request.Runtime switch
        {
            InjectionRuntimeKind.DotNet => ".NetCore",
            InjectionRuntimeKind.Mono => "Mono",
            _ => request.FrameworkVersion ?? string.Empty,
        };
    }
}