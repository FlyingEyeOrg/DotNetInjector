namespace DotNetInjector.Models;

public sealed record RemoteThreadBackendOption(
    RemoteThreadBackendKind Kind,
    string DisplayName,
    string Description);