using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DotNetInjector.Exceptions;
using DotNetInjector.Models;
using DotNetInjector.Services;
using HandyControl.Controls;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;

namespace DotNetInjector.ViewModel;

public partial class MainWindowViewModel : ObservableObject
{
    private readonly IProcessCatalogService process_catalog_service_;
    private readonly IManagedInjectorService managed_injector_service_;
    private readonly ILogger<MainWindowViewModel> logger_;

    private string process_search_text_ = string.Empty;
    private string target_process_id_text_ = string.Empty;
    private string framework_version_ = string.Empty;
    private string assembly_path_ = string.Empty;
    private string entry_class_ = string.Empty;
    private string entry_method_ = string.Empty;
    private string entry_method_argument_ = string.Empty;
    private string status_message_ = "就绪";
    private string footer_message_ = "请选择目标进程并填写注入参数。";
    private string last_standard_output_ = "标准输出将在这里显示。";
    private string last_standard_error_ = "标准错误将在这里显示。";
    private string last_execution_meta_ = "尚未执行注入。";
    private bool is_busy_;
    private TargetProcessInfo? selected_process_;
    private InjectionRuntimeOption? selected_runtime_option_;

    public ObservableCollection<TargetProcessInfo> FilteredProcesses { get; } = new();

    public ObservableCollection<string> FrameworkVersionList { get; } = new();

    public IAsyncRelayCommand RefreshProcessesCommand { get; }

    public IRelayCommand BrowseAssemblyCommand { get; }

    public IAsyncRelayCommand InjectAsyncCommand { get; }

    public IReadOnlyList<InjectionRuntimeOption> RuntimeOptions { get; } =
    [
        new(InjectionRuntimeKind.DotNetFramework, ".NET Framework", "使用统一的 ManagedInjectionLibrary.dll，并写入目标 Framework 版本。"),
        new(InjectionRuntimeKind.DotNet, ".NET / .NET Core", "使用统一的 ManagedInjectionLibrary.dll，对 .NET Core / .NET 进程注入。"),
        new(InjectionRuntimeKind.Mono, "Mono", "使用统一的 ManagedInjectionLibrary.dll，对 Mono 运行时进程注入。"),
    ];

    private List<TargetProcessInfo> all_processes_ = [];

    public string ProcessSearchText
    {
        get => process_search_text_;
        set => SetProperty(ref process_search_text_, value);
    }

    public string TargetProcessIdText
    {
        get => target_process_id_text_;
        set => SetProperty(ref target_process_id_text_, value);
    }

    public string FrameworkVersion
    {
        get => framework_version_;
        set => SetProperty(ref framework_version_, value);
    }

    public string AssemblyPath
    {
        get => assembly_path_;
        set => SetProperty(ref assembly_path_, value);
    }

    public string EntryClass
    {
        get => entry_class_;
        set => SetProperty(ref entry_class_, value);
    }

    public string EntryMethod
    {
        get => entry_method_;
        set => SetProperty(ref entry_method_, value);
    }

    public string EntryMethodArgument
    {
        get => entry_method_argument_;
        set => SetProperty(ref entry_method_argument_, value);
    }

    public string StatusMessage
    {
        get => status_message_;
        set => SetProperty(ref status_message_, value);
    }

    public string FooterMessage
    {
        get => footer_message_;
        set => SetProperty(ref footer_message_, value);
    }

    public string LastStandardOutput
    {
        get => last_standard_output_;
        set => SetProperty(ref last_standard_output_, value);
    }

    public string LastStandardError
    {
        get => last_standard_error_;
        set => SetProperty(ref last_standard_error_, value);
    }

    public string LastExecutionMeta
    {
        get => last_execution_meta_;
        set => SetProperty(ref last_execution_meta_, value);
    }

    public bool IsBusy
    {
        get => is_busy_;
        set => SetProperty(ref is_busy_, value);
    }

    public TargetProcessInfo? SelectedProcess
    {
        get => selected_process_;
        set => SetProperty(ref selected_process_, value);
    }

    public InjectionRuntimeOption? SelectedRuntimeOption
    {
        get => selected_runtime_option_;
        set => SetProperty(ref selected_runtime_option_, value);
    }

    public bool IsFrameworkVersionEnabled => SelectedRuntimeOption?.Kind == InjectionRuntimeKind.DotNetFramework;

    public int FilteredProcessCount => FilteredProcesses.Count;

    public string SelectedRuntimeDescription => SelectedRuntimeOption?.Description ?? "未选择运行时";

    public string SelectedProcessSummary => SelectedProcess is null
        ? string.IsNullOrWhiteSpace(TargetProcessIdText) ? "未选择进程" : $"手工输入 PID: {TargetProcessIdText}"
        : $"{SelectedProcess.ProcessName} ({SelectedProcess.ProcessId}) / {SelectedProcess.Architecture}";

    public string SelectedProcessPathSummary => SelectedProcess?.ExecutablePathOrFallback ?? "未选择进程路径";

    public string FrameworkVersionSummary => IsFrameworkVersionEnabled
        ? string.IsNullOrWhiteSpace(FrameworkVersion) ? "未指定" : FrameworkVersion
        : "不适用";

    public string EntrySignatureSummary
    {
        get
        {
            var trimmed_entry_class = EntryClass.Trim();
            var trimmed_entry_method = EntryMethod.Trim();
            if (string.IsNullOrWhiteSpace(trimmed_entry_class) || string.IsNullOrWhiteSpace(trimmed_entry_method))
            {
                return "未填写完整入口签名";
            }

            return $"{trimmed_entry_class}.{trimmed_entry_method}(string)";
        }
    }

    public MainWindowViewModel(
        IProcessCatalogService processCatalogService,
        IManagedInjectorService managedInjectorService,
        ILogger<MainWindowViewModel> logger)
    {
        process_catalog_service_ = processCatalogService;
        managed_injector_service_ = managedInjectorService;
        logger_ = logger;

        RefreshProcessesCommand = new AsyncRelayCommand(RefreshProcessesAsync);
        BrowseAssemblyCommand = new RelayCommand(BrowseAssembly);
        InjectAsyncCommand = new AsyncRelayCommand(InjectAsync);

        SelectedRuntimeOption = RuntimeOptions[1];
        PropertyChanged += HandlePropertyChanged;
        LoadFrameworkVersions();
        _ = RefreshProcessesAsync();
    }

    private async Task RefreshProcessesAsync()
    {
        if (IsBusy)
        {
            return;
        }

        try
        {
            IsBusy = true;
            StatusMessage = "刷新进程中";
            FooterMessage = "正在从系统枚举进程列表。";

            var previously_selected_process_id = SelectedProcess?.ProcessId;
            all_processes_ = (await process_catalog_service_.GetProcessesAsync()).ToList();
            ApplyProcessFilter();

            if (previously_selected_process_id.HasValue)
            {
                SelectedProcess = FilteredProcesses.FirstOrDefault(item => item.ProcessId == previously_selected_process_id.Value)
                    ?? all_processes_.FirstOrDefault(item => item.ProcessId == previously_selected_process_id.Value);
            }

            StatusMessage = "进程列表已更新";
            FooterMessage = $"已加载 {all_processes_.Count} 个进程。";
        }
        catch (Exception ex)
        {
            logger_.LogError(ex, "刷新进程列表失败。");
            StatusMessage = "进程刷新失败";
            FooterMessage = "无法读取进程列表。";
            MessageBox.Error($"刷新进程列表失败：{ex.Message}", "错误");
        }
        finally
        {
            IsBusy = false;
        }
    }

    private void BrowseAssembly()
    {
        var dialog = new OpenFileDialog
        {
            Title = "选择托管程序集",
            Filter = "程序集文件 (*.dll)|*.dll|所有文件 (*.*)|*.*",
            FilterIndex = 1,
            Multiselect = false,
            CheckFileExists = true,
            CheckPathExists = true,
        };

        if (dialog.ShowDialog() == true)
        {
            AssemblyPath = dialog.FileName;
            FooterMessage = $"已选择程序集：{Path.GetFileName(dialog.FileName)}";
        }
    }

    private async Task InjectAsync()
    {
        if (IsBusy)
        {
            return;
        }

        try
        {
            IsBusy = true;
            StatusMessage = "注入执行中";
            FooterMessage = "请求文件已准备，正在启动外部注入器。";
            LastStandardOutput = "等待注入器输出...";
            LastStandardError = "等待注入器错误输出...";

            var request = BuildRequest();
            var result = await managed_injector_service_.ExecuteAsync(request);

            LastStandardOutput = string.IsNullOrWhiteSpace(result.StandardOutput)
                ? "注入器没有输出标准输出。"
                : result.StandardOutput;
            LastStandardError = string.IsNullOrWhiteSpace(result.StandardError)
                ? "注入器没有输出标准错误。"
                : result.StandardError;
            LastExecutionMeta = $"ExitCode={result.ExitCode} | Duration={result.Duration.TotalMilliseconds:F0} ms | Tool={Path.GetFileName(result.ToolPath)}";

            if (result.Succeeded)
            {
                StatusMessage = "注入成功";
                FooterMessage = $"目标进程 {request.ProcessId} 注入完成。";
                MessageBox.Success($"注入成功。\n目标进程: {request.ProcessId}\n耗时: {result.Duration.TotalMilliseconds:F0} ms", "成功");
            }
            else
            {
                StatusMessage = "注入失败";
                FooterMessage = "注入器返回非零退出码。";
                MessageBox.Error($"注入失败。\n退出码: {result.ExitCode}", "失败");
            }
        }
        catch (UserFriendlyException ex)
        {
            logger_.LogWarning(ex, "注入请求被拒绝。");
            StatusMessage = "输入校验失败";
            FooterMessage = ex.Message;
            LastStandardError = string.IsNullOrWhiteSpace(ex.Details) ? ex.Message : $"{ex.Message}\n{ex.Details}";
            MessageBox.Warning(LastStandardError, "提示");
        }
        catch (Exception ex)
        {
            logger_.LogError(ex, "注入过程中发生未处理错误。");
            StatusMessage = "注入异常";
            FooterMessage = "执行期间发生异常。";
            LastStandardError = ex.ToString();
            MessageBox.Error(ex.Message, "错误");
        }
        finally
        {
            IsBusy = false;
        }
    }

    private ManagedInjectionRequest BuildRequest()
    {
        if (!int.TryParse(TargetProcessIdText, NumberStyles.None, CultureInfo.InvariantCulture, out var processId) || processId <= 0)
        {
            throw new UserFriendlyException("请输入有效的目标进程 ID。");
        }

        var runtime = SelectedRuntimeOption?.Kind ?? InjectionRuntimeKind.DotNet;

        return new ManagedInjectionRequest(
            processId,
            runtime,
            FrameworkVersion,
            AssemblyPath.Trim(),
            EntryClass.Trim(),
            EntryMethod.Trim(),
            EntryMethodArgument);
    }

    private void LoadFrameworkVersions()
    {
        FrameworkVersionList.Clear();

        foreach (var version in GetFrameworkVersions())
        {
            FrameworkVersionList.Add(version);
        }

        if (FrameworkVersionList.Count == 0)
        {
            FrameworkVersionList.Add("v4.0.30319");
        }
    }

    private void ApplyProcessFilter()
    {
        var selected_process_id = SelectedProcess?.ProcessId;
        FilteredProcesses.Clear();

        IEnumerable<TargetProcessInfo> query = all_processes_;
        if (!string.IsNullOrWhiteSpace(ProcessSearchText))
        {
            query = query.Where(item =>
                item.ProcessName.Contains(ProcessSearchText, StringComparison.OrdinalIgnoreCase)
                || item.ProcessId.ToString(CultureInfo.InvariantCulture).Contains(ProcessSearchText, StringComparison.OrdinalIgnoreCase)
                || item.MainWindowTitle.Contains(ProcessSearchText, StringComparison.OrdinalIgnoreCase));
        }

        foreach (var item in query)
        {
            FilteredProcesses.Add(item);
        }

        if (selected_process_id.HasValue)
        {
            var matching_process = FilteredProcesses.FirstOrDefault(item => item.ProcessId == selected_process_id.Value);
            if (matching_process is not null)
            {
                SelectedProcess = matching_process;
            }
        }

        OnPropertyChanged(nameof(FilteredProcessCount));
    }

    private static IEnumerable<string> GetFrameworkVersions()
    {
        var windowsFolder = Environment.GetEnvironmentVariable("windir");
        if (string.IsNullOrWhiteSpace(windowsFolder))
        {
            yield break;
        }

        var frameworkDirectory = Path.Combine(windowsFolder, "Microsoft.NET", "Framework");
        if (!Directory.Exists(frameworkDirectory))
        {
            yield break;
        }

        var directories = new DirectoryInfo(frameworkDirectory)
            .GetDirectories()
            .Where(item => item.Name.StartsWith("v", StringComparison.OrdinalIgnoreCase))
            .OrderByDescending(item => item.Name, StringComparer.OrdinalIgnoreCase);

        foreach (var directory in directories)
        {
            yield return directory.Name;
        }
    }

    private void HandlePropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        switch (e.PropertyName)
        {
            case nameof(SelectedRuntimeOption):
                if (SelectedRuntimeOption?.Kind != InjectionRuntimeKind.DotNetFramework)
                {
                    FrameworkVersion = string.Empty;
                }

                OnPropertyChanged(nameof(IsFrameworkVersionEnabled));
                OnPropertyChanged(nameof(FrameworkVersionSummary));
                OnPropertyChanged(nameof(SelectedRuntimeDescription));
                break;
            case nameof(SelectedProcess):
                if (SelectedProcess is not null)
                {
                    TargetProcessIdText = SelectedProcess.ProcessId.ToString(CultureInfo.InvariantCulture);
                    FooterMessage = $"已选择 {SelectedProcess.ProcessName} ({SelectedProcess.ProcessId})。";
                }

                OnPropertyChanged(nameof(SelectedProcessSummary));
                OnPropertyChanged(nameof(SelectedProcessPathSummary));
                break;
            case nameof(TargetProcessIdText):
                OnPropertyChanged(nameof(SelectedProcessSummary));
                break;
            case nameof(ProcessSearchText):
                ApplyProcessFilter();
                break;
            case nameof(FrameworkVersion):
                OnPropertyChanged(nameof(FrameworkVersionSummary));
                break;
            case nameof(EntryClass):
            case nameof(EntryMethod):
                OnPropertyChanged(nameof(EntrySignatureSummary));
                break;
        }
    }
}