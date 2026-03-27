using System.Diagnostics;

namespace DotNetInjector.SmokeTests;

internal static class InjectionSmokeTests
{
    public static Task RunAsync()
    {
        return InjectionValidationRunner_AllTargets_ShouldPass();
    }

    private static async Task InjectionValidationRunner_AllTargets_ShouldPass()
    {
        var repository_root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", ".."));
        var project_path = Path.Combine(repository_root, "demo", "InjectionValidationRunner", "InjectionValidationRunner.csproj");

        foreach (var build_target in new[]
                 {
                     Path.Combine(repository_root, "demo", "InjectionClassLibrary", "InjectionClassLibrary.csproj"),
                     Path.Combine(repository_root, "demo", "CoreInjectionClassLibrary", "CoreInjectionClassLibrary.csproj"),
                     Path.Combine(repository_root, "demo", "InjectionDemo", "InjectionDemo.csproj"),
                     Path.Combine(repository_root, "demo", "CoreInjectionDemo", "CoreInjectionDemo.csproj"),
                 })
        {
            await RunProcessAsync(
                "dotnet",
                $"build \"{build_target}\" -c Debug",
                repository_root,
                TimeSpan.FromMinutes(2));
        }

        var (exit_code, standard_output, standard_error) = await RunProcessAsync(
            "dotnet",
            $"run --project \"{project_path}\" -c Release -- all",
            repository_root,
            TimeSpan.FromMinutes(4));

        AssertEx.True(exit_code == 0, $"Smoke test failed. Stdout:\n{standard_output}\nStderr:\n{standard_error}");
        AssertEx.Contains("[VALIDATE] framework", standard_output, "framework validation marker missing");
        AssertEx.Contains("[VALIDATE] core", standard_output, "core validation marker missing");
        AssertEx.Contains("[VALIDATE] mono", standard_output, "mono validation marker missing");
        AssertEx.DoesNotContain("Validation failed", standard_output, "smoke output reported a validation failure");
    }

    private static async Task<(int ExitCode, string StandardOutput, string StandardError)> RunProcessAsync(
        string fileName,
        string arguments,
        string workingDirectory,
        TimeSpan timeout)
    {
        using var process = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                WorkingDirectory = workingDirectory,
            },
        };

        AssertEx.True(process.Start(), $"Failed to start process: {fileName} {arguments}");

        using var timeout_cts = new CancellationTokenSource(timeout);
        var output_task = process.StandardOutput.ReadToEndAsync(timeout_cts.Token);
        var error_task = process.StandardError.ReadToEndAsync(timeout_cts.Token);
        await process.WaitForExitAsync(timeout_cts.Token);

        return (process.ExitCode, await output_task, await error_task);
    }
}