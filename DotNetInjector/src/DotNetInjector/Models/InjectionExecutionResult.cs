namespace DotNetInjector.Models;

public sealed record InjectionExecutionResult(
    bool Succeeded,
    int ExitCode,
    string StandardOutput,
    string StandardError,
    string ToolPath,
    string PayloadPath,
    TimeSpan Duration,
    bool ToolSucceeded,
    bool ManagedExecutionSucceeded,
    string ToolExecutionSummary,
    string ManagedExecutionSummary,
    string NativeBridgeLog);