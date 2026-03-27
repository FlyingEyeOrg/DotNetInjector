namespace DotNetInjector.Models;

public sealed record InjectionRuntimeOption(
    InjectionRuntimeKind Kind,
    string DisplayName,
    string Description);