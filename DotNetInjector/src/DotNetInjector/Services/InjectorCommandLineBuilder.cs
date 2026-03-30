using DotNetInjector.Models;

namespace DotNetInjector.Services;

internal static class InjectorCommandLineBuilder
{
    public static string BuildArguments(ManagedInjectionRequest request, string payloadPath)
    {
        var arguments = $"{request.ProcessId} \"{payloadPath}\" --payload={GetPayloadArgument(request.PayloadKind)} --execution={GetExecutionArgument(request.ExecutionKind)}";

        if (request.ExecutionKind == InjectionExecutionKind.RemoteThread && request.RemoteThreadBackend.HasValue)
        {
            arguments += $" --remote-thread-backend={GetRemoteThreadBackendArgument(request.RemoteThreadBackend.Value)}";
        }

        return arguments;
    }

    private static string GetPayloadArgument(InjectionPayloadKind payloadKind)
    {
        return payloadKind switch
        {
            InjectionPayloadKind.ManualMap => "manual-map",
            _ => "load-library",
        };
    }

    private static string GetExecutionArgument(InjectionExecutionKind executionKind)
    {
        return executionKind switch
        {
            InjectionExecutionKind.ThreadContext => "thread-context",
            _ => "remote-thread",
        };
    }

    private static string GetRemoteThreadBackendArgument(RemoteThreadBackendKind backendKind)
    {
        return backendKind switch
        {
            RemoteThreadBackendKind.NtCreateThreadEx => "ntcreatethreadex",
            RemoteThreadBackendKind.CreateRemoteThread => "createremotethread",
            _ => "auto",
        };
    }
}