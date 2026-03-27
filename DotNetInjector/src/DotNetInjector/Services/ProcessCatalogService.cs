using System.Diagnostics;
using DotNetInjector.Models;

namespace DotNetInjector.Services;

public sealed class ProcessCatalogService : IProcessCatalogService
{
    public Task<IReadOnlyList<TargetProcessInfo>> GetProcessesAsync(CancellationToken cancellationToken = default)
    {
        return Task.Run<IReadOnlyList<TargetProcessInfo>>(() =>
        {
            var processes = new List<TargetProcessInfo>();

            foreach (var process in Process.GetProcesses())
            {
                cancellationToken.ThrowIfCancellationRequested();

                using (process)
                {
                    try
                    {
                        var architecture = ProcessArchitectureHelper.TryGetArchitecture(process, out var detectedArchitecture)
                            ? detectedArchitecture
                            : "unknown";

                        processes.Add(new TargetProcessInfo(
                            process.Id,
                            SafeGet(() => process.ProcessName),
                            SafeGet(() => process.MainWindowTitle),
                            SafeGetExecutablePath(process),
                            architecture));
                    }
                    catch
                    {
                    }
                }
            }

            return processes
                .OrderBy(item => item.ProcessName, StringComparer.OrdinalIgnoreCase)
                .ThenBy(item => item.ProcessId)
                .ToList();
        }, cancellationToken);
    }

    private static string SafeGet(Func<string> accessor)
    {
        try
        {
            return accessor() ?? string.Empty;
        }
        catch
        {
            return string.Empty;
        }
    }

    private static string SafeGetExecutablePath(Process process)
    {
        try
        {
            return process.MainModule?.FileName ?? string.Empty;
        }
        catch
        {
            return string.Empty;
        }
    }
}