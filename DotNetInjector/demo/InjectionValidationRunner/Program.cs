using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text;
using DotNetInjector.Models;
using DotNetInjector.Services;
using Microsoft.Extensions.Logging;

var dotnet_root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", ".."));

var targets = new[]
{
    new ValidationTarget(
        "framework",
        InjectionRuntimeKind.DotNetFramework,
        "v4.0.30319",
        file_name: Path.Combine(dotnet_root, "demo", "InjectionDemo", "bin", "Debug", "InjectionDemo.exe"),
        arguments: "--non-interactive",
        assembly_path: Path.Combine(dotnet_root, "demo", "InjectionClassLibrary", "bin", "Debug", "InjectionClassLibrary.dll"),
        entry_class: "InjectionClassLibrary.InjectionClass",
        entry_method: "InjectionMethod",
        entry_argument: "framework-validation",
        expected_markers: ["print value: framework-validation", "Entry Assembly: InjectionDemo"]),
    new ValidationTarget(
        "core",
        InjectionRuntimeKind.DotNet,
        framework_version: null,
        file_name: Path.Combine(dotnet_root, "demo", "CoreInjectionDemo", "bin", "Debug", "net8.0", "CoreInjectionDemo.exe"),
        arguments: "--non-interactive",
        assembly_path: Path.Combine(dotnet_root, "demo", "CoreInjectionClassLibrary", "bin", "Debug", "net8.0", "CoreInjectionClassLibrary.dll"),
        entry_class: "CoreInjectionClassLibrary.InjectionClass",
        entry_method: "InjectionMethod",
        entry_argument: "core-validation",
        expected_markers: ["print value: core-validation", "Entry Assembly: CoreInjectionDemo"]),
    new ValidationTarget(
        "mono",
        InjectionRuntimeKind.Mono,
        framework_version: null,
        file_name: @"C:\Program Files\Mono\bin\mono.exe",
        arguments: $"\"{Path.Combine(dotnet_root, "demo", "InjectionDemo", "bin", "Debug", "InjectionDemo.exe")}\" --non-interactive",
        assembly_path: Path.Combine(dotnet_root, "demo", "InjectionClassLibrary", "bin", "Debug", "InjectionClassLibrary.dll"),
        entry_class: "InjectionClassLibrary.InjectionClass",
        entry_method: "InjectionMethod",
        entry_argument: "mono-validation",
        expected_markers: ["print value: mono-validation", "Entry Assembly: InjectionDemo"]),
};

var selected_names = args.Length == 0 || args.Contains("all", StringComparer.OrdinalIgnoreCase)
    ? new HashSet<string>(targets.Select(target => target.name), StringComparer.OrdinalIgnoreCase)
    : new HashSet<string>(args, StringComparer.OrdinalIgnoreCase);

var selected_targets = targets.Where(target => selected_names.Contains(target.name)).ToArray();
if (selected_targets.Length == 0)
{
    Console.Error.WriteLine("No validation target selected. Use: framework | core | mono | all");
    return 1;
}

using var logger_factory = LoggerFactory.Create(builder => builder.SetMinimumLevel(LogLevel.Information));
var injector_service = new ManagedInjectorService(logger_factory.CreateLogger<ManagedInjectorService>());

var failures = 0;
foreach (var target in selected_targets)
{
    Console.WriteLine($"[VALIDATE] {target.name}");

    if (!File.Exists(target.file_name))
    {
        Console.Error.WriteLine($"Missing target executable: {target.file_name}");
        failures++;
        continue;
    }

    if (!File.Exists(target.assembly_path))
    {
        Console.Error.WriteLine($"Missing injection assembly: {target.assembly_path}");
        failures++;
        continue;
    }

    await using var session = await TargetProcessSession.StartAsync(target);
    var request = new ManagedInjectionRequest(
        session.process_id,
        target.runtime,
        target.framework_version,
        target.assembly_path,
        target.entry_class,
        target.entry_method,
        target.entry_argument);

    try
    {
        var result = await injector_service.ExecuteAsync(request);
        Console.WriteLine($"  Injector exit: {result.ExitCode}");
        Console.WriteLine($"  Tool: {result.ToolPath}");
        if (!string.IsNullOrWhiteSpace(result.StandardOutput))
        {
            Console.WriteLine("  Injector stdout:");
            Console.WriteLine(result.StandardOutput);
        }
        if (!string.IsNullOrWhiteSpace(result.StandardError))
        {
            Console.WriteLine("  Injector stderr:");
            Console.WriteLine(result.StandardError);
        }

        await Task.Delay(TimeSpan.FromSeconds(2));
        var full_output = session.GetCombinedOutput();

        var missing_markers = target.expected_markers
            .Where(marker => !full_output.Contains(marker, StringComparison.Ordinal))
            .ToArray();

        if (!result.Succeeded || missing_markers.Length != 0)
        {
            failures++;
            Console.Error.WriteLine($"  Validation failed for {target.name}.");
            if (missing_markers.Length != 0)
            {
                Console.Error.WriteLine("  Missing markers:");
                foreach (var marker in missing_markers)
                {
                    Console.Error.WriteLine($"    {marker}");
                }
            }

            Console.Error.WriteLine("  Target output:");
            Console.Error.WriteLine(full_output);
            continue;
        }

        Console.WriteLine("  Validation passed.");
        Console.WriteLine("  Target output:");
        Console.WriteLine(full_output);
    }
    catch (Exception exception)
    {
        failures++;
        Console.Error.WriteLine($"  Validation exception: {exception}");
    }
}

return failures == 0 ? 0 : 1;

sealed record ValidationTarget(
    string name,
    InjectionRuntimeKind runtime,
    string? framework_version,
    string file_name,
    string arguments,
    string assembly_path,
    string entry_class,
    string entry_method,
    string entry_argument,
    string[] expected_markers);

sealed class TargetProcessSession : IAsyncDisposable
{
    private readonly Process process_;
    private readonly ConcurrentQueue<string> output_lines_ = new();
    private readonly ConcurrentQueue<string> error_lines_ = new();
    private readonly TaskCompletionSource<int> process_id_source_ = new(TaskCreationOptions.RunContinuationsAsynchronously);

    public int process_id => process_id_source_.Task.Result;

    private TargetProcessSession(Process process)
    {
        process_ = process;
        process_.OutputDataReceived += HandleOutput;
        process_.ErrorDataReceived += HandleError;
    }

    public static async Task<TargetProcessSession> StartAsync(ValidationTarget target)
    {
        var process = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = target.file_name,
                Arguments = target.arguments,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                WorkingDirectory = Path.GetDirectoryName(target.file_name) ?? AppContext.BaseDirectory,
            },
            EnableRaisingEvents = true,
        };

        var session = new TargetProcessSession(process);
        if (!process.Start())
        {
            throw new InvalidOperationException($"Failed to start target process: {target.file_name}");
        }

        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        await using var registration = timeout.Token.Register(() =>
            session.process_id_source_.TrySetException(new TimeoutException("Timed out waiting for target PID output.")));

        await session.process_id_source_.Task;
        return session;
    }

    public string GetCombinedOutput()
    {
        var builder = new StringBuilder();
        foreach (var line in output_lines_)
        {
            builder.AppendLine(line);
        }
        foreach (var line in error_lines_)
        {
            builder.AppendLine("[stderr] " + line);
        }
        return builder.ToString().Trim();
    }

    public ValueTask DisposeAsync()
    {
        try
        {
            process_.OutputDataReceived -= HandleOutput;
            process_.ErrorDataReceived -= HandleError;
            if (!process_.HasExited)
            {
                process_.Kill(entireProcessTree: true);
                process_.WaitForExit(5000);
            }
        }
        catch
        {
        }
        finally
        {
            process_.Dispose();
        }

        return ValueTask.CompletedTask;
    }

    private void HandleOutput(object sender, DataReceivedEventArgs event_args)
    {
        if (string.IsNullOrWhiteSpace(event_args.Data))
        {
            return;
        }

        output_lines_.Enqueue(event_args.Data);
        const string prefix = "process id: ";
        if (event_args.Data.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(event_args.Data[prefix.Length..].Trim(), out var process_id))
        {
            process_id_source_.TrySetResult(process_id);
        }
    }

    private void HandleError(object sender, DataReceivedEventArgs event_args)
    {
        if (string.IsNullOrWhiteSpace(event_args.Data))
        {
            return;
        }

        error_lines_.Enqueue(event_args.Data);
    }
}