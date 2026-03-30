namespace DotNetInjector.Models;

public enum RemoteThreadBackendKind
{
    Auto,
    NtCreateThreadEx,
    CreateRemoteThread,
}