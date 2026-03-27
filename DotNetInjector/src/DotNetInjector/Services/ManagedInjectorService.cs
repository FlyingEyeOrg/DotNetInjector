using System.Diagnostics;
using System.IO;
using DotNetInjector.Exceptions;
using DotNetInjector.Models;
using Microsoft.Extensions.Logging;

namespace DotNetInjector.Services;

public sealed class ManagedInjectorService : IManagedInjectorService
{
    private static readonly TimeSpan k_execution_timeout = TimeSpan.FromSeconds(30);

    private readonly ILogger<ManagedInjectorService> logger_;

    public ManagedInjectorService(ILogger<ManagedInjectorService> logger)
    {
        logger_ = logger;
    }

    public async Task<InjectionExecutionResult> ExecuteAsync(ManagedInjectionRequest request, CancellationToken cancellationToken = default)
    {
        ValidateRequest(request);

        using var target_process = Process.GetProcessById(request.ProcessId);
        if (target_process.HasExited)
        {
            throw new UserFriendlyException("目标进程已经退出，请刷新进程列表后重试。");
        }

        if (!ProcessArchitectureHelper.TryGetArchitecture(target_process, out var architecture))
        {
            throw new UserFriendlyException("无法识别目标进程位数，请使用管理员权限重试。", details: $"ProcessId={request.ProcessId}");
        }

        var (tool_path, payload_path) = InjectorAssetResolver.Resolve(request.Runtime, architecture);
        if (!File.Exists(tool_path))
        {
            throw new UserFriendlyException($"注入器不存在：{tool_path}");
        }

        if (!File.Exists(payload_path))
        {
            throw new UserFriendlyException($"桥接注入库不存在：{payload_path}");
        }

        using var bridge = new SharedMemoryInjectionBridge();
        bridge.Publish(request);

        var stopwatch = Stopwatch.StartNew();
        using var timeout_cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout_cts.CancelAfter(k_execution_timeout);

        var process_info = new ProcessStartInfo
        {
            FileName = tool_path,
            Arguments = $"{request.ProcessId} \"{payload_path}\" --payload=load-library --execution=remote-thread",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
            WorkingDirectory = Path.GetDirectoryName(tool_path) ?? AppContext.BaseDirectory,
        };

        logger_.LogInformation("启动注入器: {ToolPath} {Arguments}", process_info.FileName, process_info.Arguments);

        using var injector_process = Process.Start(process_info);
        if (injector_process is null)
        {
            throw new UserFriendlyException("启动注入器失败，请检查执行权限。", details: tool_path);
        }

        var output_task = injector_process.StandardOutput.ReadToEndAsync(timeout_cts.Token);
        var error_task = injector_process.StandardError.ReadToEndAsync(timeout_cts.Token);

        try
        {
            await injector_process.WaitForExitAsync(timeout_cts.Token);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            TryKillProcess(injector_process);
            throw new UserFriendlyException("注入器执行超时，目标进程可能无响应或桥接 DLL 未完成初始化。", details: $"Timeout={k_execution_timeout.TotalSeconds}s");
        }

        var standard_output = await output_task;
        var standard_error = await error_task;
        stopwatch.Stop();

        logger_.LogInformation(
            "注入器执行完成: ExitCode={ExitCode}, Duration={Duration}ms",
            injector_process.ExitCode,
            stopwatch.ElapsedMilliseconds);

        return new InjectionExecutionResult(
            injector_process.ExitCode == 0,
            injector_process.ExitCode,
            standard_output.Trim(),
            standard_error.Trim(),
            tool_path,
            payload_path,
            stopwatch.Elapsed);
    }

    private static void ValidateRequest(ManagedInjectionRequest request)
    {
        if (request.ProcessId <= 0)
        {
            throw new UserFriendlyException("请输入有效的目标进程 ID。");
        }

        if (string.IsNullOrWhiteSpace(request.AssemblyPath))
        {
            throw new UserFriendlyException("请选择要注入的托管程序集。");
        }

        if (!File.Exists(request.AssemblyPath))
        {
            throw new UserFriendlyException($"托管程序集不存在：{request.AssemblyPath}");
        }

        if (!string.Equals(Path.GetExtension(request.AssemblyPath), ".dll", StringComparison.OrdinalIgnoreCase))
        {
            throw new UserFriendlyException("托管程序集必须是 DLL 文件。");
        }

        if (string.IsNullOrWhiteSpace(request.EntryClass))
        {
            throw new UserFriendlyException("请输入入口类型全名。");
        }

        if (string.IsNullOrWhiteSpace(request.EntryMethod))
        {
            throw new UserFriendlyException("请输入入口方法名称。");
        }

        if (request.Runtime == InjectionRuntimeKind.DotNetFramework && string.IsNullOrWhiteSpace(request.FrameworkVersion))
        {
            throw new UserFriendlyException(".NET Framework 注入必须提供对应的运行时版本。", details: "例如 v4.0.30319");
        }
    }

    private static void TryKillProcess(Process process)
    {
        try
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
            }
        }
        catch
        {
        }
    }
}