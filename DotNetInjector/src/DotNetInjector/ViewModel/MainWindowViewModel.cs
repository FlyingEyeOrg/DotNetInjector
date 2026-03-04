using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DotNetInjector.Utils;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;
using System.IO;
using System.Windows;
using System.Windows.Input;

namespace DotNetInjector.ViewModel
{
    public class MainWindowViewModel : ObservableObject
    {
        #region 命令

        public ICommand SelectFileCommand { get; }

        public ICommand InjectCommand { get; }

        #endregion

        #region 需要属性通知的属性

        private string? _assemblyPath;

        /// <summary>
        /// 程序集文件路径（仅限 .dll 文件）
        /// </summary>
        public string? AssemblyPath
        {
            get => _assemblyPath;
            set => SetProperty(ref _assemblyPath, value);
        }

        #endregion

        #region 不需要属性通知的属性（普通自动属性）

        /// <summary>
        /// 选择的 .NET Framework 版本
        /// </summary>
        public string? FrameworkVersion { get; set; }

        /// <summary>
        /// 入口类名
        /// </summary>
        public string? EntryClass { get; set; }

        /// <summary>
        /// 入口方法名
        /// </summary>
        public string? EntryMethod { get; set; }

        /// <summary>
        /// 入口方法参数
        /// </summary>
        public string? EntryMethodArgument { get; set; }

        /// <summary>
        ///  目标进程 id
        /// </summary>
        public int? ProcessId { get; set; }

        /// <summary>
        /// .NET Framework 版本列表
        /// </summary>
        public List<string> FrameworkVersionList { get; } = new List<string> { ".NetCore", "Mono" };

        #endregion

        #region 私有字段

        private readonly SharedMemWriter _frameworkVersionMem;
        private readonly SharedMemWriter _assemblyPathMem;
        private readonly SharedMemWriter _entryClassMem;
        private readonly SharedMemWriter _entryMethodMem;
        private readonly SharedMemWriter _entryMethodArgumentMem;

        private readonly ILogger<MainWindowViewModel> _logger;

        #endregion

        #region 构造函数

        public MainWindowViewModel(ILogger<MainWindowViewModel> logger)
        {
            _logger = logger;

            // 初始化共享内存写入器
            _frameworkVersionMem = new SharedMemWriter();
            _assemblyPathMem = new SharedMemWriter();
            _entryClassMem = new SharedMemWriter();
            _entryMethodMem = new SharedMemWriter();
            _entryMethodArgumentMem = new SharedMemWriter();

            // 初始化共享内存
            InitializeSharedMemory();

            // 加载框架版本列表
            LoadFrameworkVersions();

            // 初始化命令
            SelectFileCommand = new RelayCommand(SelectFile);
            InjectCommand = new RelayCommand(Inject);

            _logger.LogInformation("MainWindowViewModel 初始化完成。");
        }

        #endregion

        #region 私有方法

        /// <summary>
        /// 初始化共享内存
        /// </summary>
        private void InitializeSharedMemory()
        {
            try
            {
                _frameworkVersionMem.Create("FrameworkVersion", 128 * 2);
                _assemblyPathMem.Create("AssemblyFile", 256 * 2);
                _entryClassMem.Create("EntryClass", 128 * 2);
                _entryMethodMem.Create("EntryMethod", 128 * 2);
                _entryMethodArgumentMem.Create("EntryArgument", 256 * 2);

                _logger.LogInformation("共享内存初始化成功。");
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "初始化共享内存失败。");
                MessageBox.Show(
                    "初始化共享内存失败，请检查权限或共享内存是否被占用。",
                    "错误",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }
        }

        /// <summary>
        /// 加载 .NET Framework 版本列表
        /// </summary>
        private void LoadFrameworkVersions()
        {
            try
            {
                var versions = GetFrameworkVersionList();
                FrameworkVersionList.InsertRange(0, versions);
                _logger.LogInformation("已加载 {Count} 个框架版本。", FrameworkVersionList.Count);
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "加载框架版本列表失败。");
                // 失败时使用默认列表，不提示用户
            }
        }

        /// <summary>
        /// 获取系统中已安装的 .NET Framework 版本
        /// </summary>
        private List<string> GetFrameworkVersionList()
        {
            var versions = new List<string>();

            var windowsFolder = Environment.GetEnvironmentVariable("windir");

            if (string.IsNullOrWhiteSpace(windowsFolder))
            {
                return versions;
            }

            var frameworkDir = Path.Combine(windowsFolder, "Microsoft.NET\\Framework");

            if (!Directory.Exists(frameworkDir))
            {
                return versions;
            }

            var dirInfo = new DirectoryInfo(frameworkDir);

            return dirInfo.GetDirectories()
                .Where(item => item.Name.StartsWith("v", StringComparison.OrdinalIgnoreCase))
                .Select(item => item.Name)
                .ToList();
        }

        /// <summary>
        /// 打开文件对话框选择 .dll 文件
        /// </summary>
        private void SelectFile()
        {
            try
            {
                var dialog = new OpenFileDialog
                {
                    Title = "选择 .NET 程序集文件",
                    Filter = "程序集文件 (*.dll)|*.dll|所有文件 (*.*)|*.*",
                    FilterIndex = 1,
                    Multiselect = false,
                    CheckFileExists = true,
                    CheckPathExists = true
                };

                var result = dialog.ShowDialog();

                if (result == true && !string.IsNullOrWhiteSpace(dialog.FileName))
                {
                    // 二次验证文件扩展名
                    if (!string.Equals(Path.GetExtension(dialog.FileName), ".dll", StringComparison.OrdinalIgnoreCase))
                    {
                        MessageBox.Show(
                            "请选择 .dll 文件。",
                            "文件类型错误",
                            MessageBoxButton.OK,
                            MessageBoxImage.Warning);
                        return;
                    }

                    AssemblyPath = dialog.FileName;
                    _logger.LogInformation("已选择程序集文件: {Path}", dialog.FileName);
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "打开文件对话框失败。");
                MessageBox.Show(
                    "打开文件对话框失败，请检查系统文件对话框是否正常。",
                    "错误",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }
        }

        /// <summary>
        /// 保存数据到共享内存，然后注入目标进程
        /// </summary>
        private void Inject()
        {
            try
            {
                // 验证必填参数
                if (string.IsNullOrWhiteSpace(AssemblyPath))
                {
                    MessageBox.Show(
                        "请选择程序集文件。",
                        "参数缺失",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning);
                    return;
                }

                if (string.IsNullOrWhiteSpace(EntryClass))
                {
                    MessageBox.Show(
                        "请输入入口类名。",
                        "参数缺失",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning);
                    return;
                }

                if (string.IsNullOrWhiteSpace(EntryMethod))
                {
                    MessageBox.Show(
                        "请输入入口方法名。",
                        "参数缺失",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning);
                    return;
                }

                // 验证 ProcessId（必填）
                if (!ProcessId.HasValue || ProcessId.Value <= 0)
                {
                    MessageBox.Show(
                        "请输入有效的目标进程 ID。",
                        "参数缺失",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning);
                    return;
                }

                // 验证 ProcessId 对应的进程是否存在
                if (!System.Diagnostics.Process.GetProcesses().Any(p => p.Id == ProcessId.Value))
                {
                    MessageBox.Show(
                        $"进程 ID {ProcessId.Value} 不存在，请检查后重试。",
                        "进程不存在",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning);
                    return;
                }

                // 写入共享内存
                _frameworkVersionMem.Write(FrameworkVersion ?? string.Empty);
                _assemblyPathMem.Write(AssemblyPath);
                _entryClassMem.Write(EntryClass);
                _entryMethodMem.Write(EntryMethod);
                _entryMethodArgumentMem.Write(EntryMethodArgument ?? string.Empty);

                _logger.LogInformation("数据已成功写入共享内存，准备注入进程 {ProcessId}。", ProcessId.Value);
                MessageBox.Show(
                    $"数据已成功写入共享内存！\n目标进程: {ProcessId.Value}",
                    "保存成功",
                    MessageBoxButton.OK,
                    MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "保存数据到共享内存失败。");
                MessageBox.Show(
                    "保存数据失败，请检查共享内存是否可用或是否被占用。",
                    "错误",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }
        }

        #endregion
    }
}