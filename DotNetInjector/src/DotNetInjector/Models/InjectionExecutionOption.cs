namespace DotNetInjector.Models;

public sealed record InjectionExecutionOption(
    InjectionExecutionKind Kind,
    string DisplayName,
    string Description);