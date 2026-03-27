using System.Diagnostics;
using System.IO;
using System.Text;
using DotNetInjector.Exceptions;
using DotNetInjector.Models;
using Microsoft.Extensions.Logging;

namespace DotNetInjector.Services;

public sealed class ManagedInjectorService : IManagedInjectorService
{
    private static readonly TimeSpan k_execution_timeout = TimeSpan.FromSeconds(30);
    private static readonly TimeSpan k_request_file_grace_period = TimeSpan.FromMilliseconds(750);
    private const string k_native_log_directory_name = "[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-InjectionLogs";
    private const string k_native_log_file_name = "injection.log";

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

        using var bridge = InjectionRequestFileBridge.Publish(
            request,
            Path.GetDirectoryName(payload_path));

        var native_log_snapshot = CaptureNativeLogSnapshot();

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

        logger_.LogInformation(
            "启动注入器: Tool={ToolPath}, Payload={PayloadPath}, TargetArch={TargetArchitecture}, RequestFiles={RequestFiles}, Args={Arguments}",
            process_info.FileName,
            payload_path,
            architecture,
            string.Join(" | ", bridge.PublishedRequestFilePaths),
            process_info.Arguments);

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

        if (injector_process.ExitCode == 0)
        {
            await Task.Delay(k_request_file_grace_period, cancellationToken);
        }

        var standard_output = await output_task;
        var standard_error = await error_task;
        var native_bridge_log = ReadNativeLogDelta(native_log_snapshot);
        stopwatch.Stop();

        var tool_succeeded = injector_process.ExitCode == 0;
        var managed_execution_succeeded = tool_succeeded && DetermineManagedExecutionSucceeded(native_bridge_log);
        var tool_execution_summary = tool_succeeded
            ? "DLL 注入成功"
            : $"DLL 注入失败，退出码 {injector_process.ExitCode}";
        var managed_execution_summary = GetManagedExecutionSummary(tool_succeeded, managed_execution_succeeded, native_bridge_log);

        logger_.LogInformation(
            "注入器执行完成: ExitCode={ExitCode}, Duration={Duration}ms",
            injector_process.ExitCode,
            stopwatch.ElapsedMilliseconds);

        return new InjectionExecutionResult(
            tool_succeeded && managed_execution_succeeded,
            injector_process.ExitCode,
            standard_output.Trim(),
            standard_error.Trim(),
            tool_path,
            payload_path,
            stopwatch.Elapsed,
            tool_succeeded,
            managed_execution_succeeded,
            tool_execution_summary,
            managed_execution_summary,
            native_bridge_log.Trim());
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

    private static string GetNativeLogFilePath()
    {
        return Path.Combine(Path.GetTempPath(), k_native_log_directory_name, k_native_log_file_name);
    }

    private static long CaptureNativeLogSnapshot()
    {
        var log_file_path = GetNativeLogFilePath();
        if (!File.Exists(log_file_path))
        {
            return 0;
        }

        return new FileInfo(log_file_path).Length;
    }

    private static string ReadNativeLogDelta(long snapshotLength)
    {
        var log_file_path = GetNativeLogFilePath();
        if (!File.Exists(log_file_path))
        {
            return string.Empty;
        }

        using var stream = new FileStream(log_file_path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (snapshotLength > 0 && snapshotLength <= stream.Length)
        {
            stream.Seek(snapshotLength, SeekOrigin.Begin);
        }
        else
        {
            stream.Seek(0, SeekOrigin.Begin);
        }

        using var reader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
        return reader.ReadToEnd();
    }

    private static bool DetermineManagedExecutionSucceeded(string nativeBridgeLog)
    {
        if (string.IsNullOrWhiteSpace(nativeBridgeLog))
        {
            return false;
        }

        return nativeBridgeLog.Contains("Framework managed method executed successfully", StringComparison.Ordinal)
            || nativeBridgeLog.Contains("CoreCLR managed method executed successfully", StringComparison.Ordinal)
            || nativeBridgeLog.Contains("Mono managed method executed successfully", StringComparison.Ordinal);
    }

    private static string GetManagedExecutionSummary(bool toolSucceeded, bool managedExecutionSucceeded, string nativeBridgeLog)
    {
        if (!toolSucceeded)
        {
            return "未进入托管执行阶段";
        }

        if (managedExecutionSucceeded)
        {
            return "托管入口执行成功";
        }

        if (string.IsNullOrWhiteSpace(nativeBridgeLog))
        {
            return "未确认托管入口执行结果";
        }

        var normalized_log = nativeBridgeLog.Replace("\r", string.Empty, StringComparison.Ordinal).Trim();
        var log_lines = normalized_log.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var last_log_line = log_lines.Length == 0 ? "桥接层未提供错误细节" : log_lines[^1];
        return $"托管入口执行失败: {last_log_line}";
    }
}