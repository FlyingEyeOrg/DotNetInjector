namespace DotNetInjector.Models;

public sealed record ManagedInjectionRequest(
    int ProcessId,
    InjectionRuntimeKind Runtime,
    string? FrameworkVersion,
    string AssemblyPath,
    string EntryClass,
    string EntryMethod,
    string EntryArgument);