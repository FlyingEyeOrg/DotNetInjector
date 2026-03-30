namespace DotNetInjector.Models;

public sealed record InjectionPayloadOption(
    InjectionPayloadKind Kind,
    string DisplayName,
    string Description);