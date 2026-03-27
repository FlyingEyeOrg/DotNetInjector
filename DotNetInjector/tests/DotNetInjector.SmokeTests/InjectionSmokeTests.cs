using System.Diagnostics;
using System.IO;
using System.Text;

namespace DotNetInjector.SmokeTests;

internal static class InjectionSmokeTests
{
    private static readonly string[] k_build_targets =
    [
        Path.Combine("demo", "InjectionClassLibrary", "InjectionClassLibrary.csproj"),
        Path.Combine("demo", "CoreInjectionClassLibrary", "CoreInjectionClassLibrary.csproj"),
        Path.Combine("demo", "InjectionDemo", "InjectionDemo.csproj"),
        Path.Combine("demo", "CoreInjectionDemo", "CoreInjectionDemo.csproj"),
    ];

    public static async Task RunAsync()
    {
        var repository_root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", ".."));
        await BuildValidationAssetsAsync(repository_root);

        await InjectionValidationTarget_ShouldPass(repository_root, "framework", "方法强制调用成功");
        await InjectionValidationTarget_ShouldPass(repository_root, "core", "方法强制调用成功");
        await InjectionValidationTarget_ShouldPass(repository_root, "mono", "方法强制调用成功");

        await UiSmokeTests.RunAsync(repository_root);
    }

    private static async Task BuildValidationAssetsAsync(string repositoryRoot)
    {
        foreach (var build_target in k_build_targets)
        {
            await RunProcessAsync(
                "dotnet",
                $"build \"{Path.Combine(repositoryRoot, build_target)}\" -c Debug",
                repositoryRoot,
                TimeSpan.FromMinutes(2));
        }

        await RunProcessAsync(
            "dotnet",
            $"build \"{Path.Combine(repositoryRoot, "src", "DotNetInjector", "DotNetInjector.csproj")}\" -c Debug",
            repositoryRoot,
            TimeSpan.FromMinutes(2));
    }

    private static async Task InjectionValidationTarget_ShouldPass(string repositoryRoot, string targetName, params string[] expectedMarkers)
    {
        var project_path = Path.Combine(repositoryRoot, "demo", "InjectionValidationRunner", "InjectionValidationRunner.csproj");

        var (exit_code, standard_output, standard_error) = await RunProcessAsync(
            "dotnet",
            $"run --project \"{project_path}\" -c Release -- {targetName}",
            repositoryRoot,
            TimeSpan.FromMinutes(4));

        AssertEx.True(exit_code == 0, $"Smoke test failed. Stdout:\n{standard_output}\nStderr:\n{standard_error}");
        AssertEx.Contains($"[VALIDATE] {targetName}", standard_output, $"{targetName} validation marker missing");
        AssertEx.DoesNotContain("Validation failed", standard_output, "smoke output reported a validation failure");

        foreach (var expected_marker in expectedMarkers)
        {
            AssertEx.Contains(expected_marker, standard_output, $"missing marker for {targetName}: {expected_marker}");
        }
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
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8,
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