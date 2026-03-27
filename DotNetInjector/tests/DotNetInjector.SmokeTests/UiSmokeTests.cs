using System.Diagnostics;
using System.IO;
using System.Windows.Automation;

namespace DotNetInjector.SmokeTests;

internal static class UiSmokeTests
{
    public static async Task RunAsync(string repositoryRoot)
    {
        var app_path = Path.Combine(
            repositoryRoot,
            "src",
            "DotNetInjector",
            "bin",
            "Debug",
            "net8.0-windows",
            "DotNetInjector.exe");

        AssertEx.True(File.Exists(app_path), $"Main application was not built: {app_path}");

        using var process = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = app_path,
                WorkingDirectory = Path.GetDirectoryName(app_path) ?? repositoryRoot,
                UseShellExecute = true,
            },
        };

        AssertEx.True(process.Start(), "Failed to start DotNetInjector UI process.");

        try
        {
            var main_window = await WaitForMainWindowAsync(process, TimeSpan.FromSeconds(20));
            var refresh_button = FindRequiredElement(main_window, "RefreshProcessesButton", ControlType.Button);
            var process_search_text_box = FindRequiredElement(main_window, "ProcessSearchTextBox", ControlType.Edit);
            var process_count_text = FindRequiredElement(main_window, "ProcessCountTextBlock");
            var inject_button = FindRequiredElement(main_window, "InjectButton", ControlType.Button);
            var footer_message = FindRequiredElement(main_window, "FooterMessageTextBlock");

            AssertEx.True(refresh_button.TryGetCurrentPattern(InvokePattern.Pattern, out var refresh_pattern), "Refresh button does not support invoke pattern.");
            ((InvokePattern)refresh_pattern).Invoke();

            AssertEx.True(process_search_text_box.TryGetCurrentPattern(ValuePattern.Pattern, out var value_pattern), "Process search text box does not support value pattern.");
            ((ValuePattern)value_pattern).SetValue("dotnet");
            await Task.Delay(500);
            ((ValuePattern)value_pattern).SetValue(string.Empty);

            AssertEx.True(inject_button.Current.IsEnabled, "Inject button should be enabled once the window has loaded.");
            AssertEx.True(!string.IsNullOrWhiteSpace(process_count_text.Current.Name) || !string.IsNullOrWhiteSpace(process_count_text.Current.AutomationId), "Process count text was not available through UI automation.");
            AssertEx.True(!string.IsNullOrWhiteSpace(footer_message.Current.Name) || !string.IsNullOrWhiteSpace(footer_message.Current.AutomationId), "Footer message text was not available through UI automation.");
        }
        finally
        {
            CloseProcess(process);
        }
    }

    private static async Task<AutomationElement> WaitForMainWindowAsync(Process process, TimeSpan timeout)
    {
        try
        {
            process.WaitForInputIdle(5000);
        }
        catch
        {
        }

        var started_at = Stopwatch.StartNew();
        while (started_at.Elapsed < timeout)
        {
            process.Refresh();
            if (process.HasExited)
            {
                throw new InvalidOperationException("DotNetInjector UI process exited before the main window was ready.");
            }

            if (process.MainWindowHandle != IntPtr.Zero)
            {
                return AutomationElement.FromHandle(process.MainWindowHandle);
            }

            await Task.Delay(250);
        }

        throw new TimeoutException("Timed out waiting for the DotNetInjector main window.");
    }

    private static AutomationElement FindRequiredElement(AutomationElement root, string automationId, ControlType? controlType = null)
    {
        var conditions = new List<Condition>
        {
            new PropertyCondition(AutomationElement.AutomationIdProperty, automationId),
        };

        if (controlType is not null)
        {
            conditions.Add(new PropertyCondition(AutomationElement.ControlTypeProperty, controlType));
        }

        var condition = conditions.Count == 1 ? conditions[0] : new AndCondition(conditions.ToArray());
        var element = root.FindFirst(TreeScope.Descendants, condition);
        if (element is not null)
        {
            return element;
        }

        throw new InvalidOperationException($"Unable to find UI element: {automationId}");
    }

    private static void CloseProcess(Process process)
    {
        try
        {
            if (process.HasExited)
            {
                return;
            }

            process.CloseMainWindow();
            if (process.WaitForExit(5000))
            {
                return;
            }

            process.Kill(entireProcessTree: true);
            process.WaitForExit(5000);
        }
        catch
        {
        }
    }
}