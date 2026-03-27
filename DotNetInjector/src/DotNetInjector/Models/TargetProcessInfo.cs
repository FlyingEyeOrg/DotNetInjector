namespace DotNetInjector.Models;

public sealed record TargetProcessInfo(
    int ProcessId,
    string ProcessName,
    string MainWindowTitle,
    string ExecutablePath,
    string Architecture)
{
    public string DisplayName => $"{ProcessName} ({ProcessId})";

    public string WindowTitleOrFallback => string.IsNullOrWhiteSpace(MainWindowTitle)
        ? "无窗口标题"
        : MainWindowTitle;

    public string ExecutablePathOrFallback => string.IsNullOrWhiteSpace(ExecutablePath)
        ? "无法读取进程路径"
        : ExecutablePath;
}