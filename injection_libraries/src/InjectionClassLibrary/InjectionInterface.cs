using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;

namespace InjectedClassLibrary
{
    /// <summary>
    /// 注入接口类 - 提供动态程序集加载和方法调用功能
    /// 优化版本：简化生命周期管理，增强配置灵活性
    /// </summary>
    public sealed class InjectionInterface : IDisposable
    {
        private bool _disposed = false;
        private static readonly Lazy<string> DefaultConfigPath = new Lazy<string>(GetDefaultConfigPath);

        /// <summary>
        /// 静态注入方法 - 单次使用工具，自动资源管理
        /// </summary>
        /// <param name="configFile">可选的配置文件路径，为空则使用默认路径</param>
        /// <returns>执行结果：0=成功，负数=失败</returns>
        public static int Inject(string? configFile = null)
        {
            Logger.Log($"Static Inject method called with configFile: '{configFile ?? "null (using default)"}'");

            // 使用 using 语句确保资源正确释放，但通过配置延迟清理时机
            using var injector = new InjectionInterface();

            try
            {
                return injector.ExecuteInjection(configFile);
            }
            catch (Exception ex)
            {
                Logger.Log($"Inject operation failed", ex);
                return -1;
            }
        }

        /// <summary>
        /// 执行注入的核心方法
        /// </summary>
        private int ExecuteInjection(string? configFile)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(InjectionInterface));

            Logger.Log("Starting injection process");

            try
            {
                // 获取配置（智能选择：参数优先，无参数则用默认）
                var config = GetConfiguration(configFile);
                Logger.Log($"Using configuration - Assembly: {config.TargetAssemblyPath}, Type: {config.TargetTypeName}, Method: {config.TargetMethodName}");

                // 验证配置
                ValidateConfiguration(config);

                // 加载依赖
                LoadDependencyAssemblies(config.DependencyAssemblyPaths);

                // 执行目标方法
                ExecuteTargetMethod(config);

                Logger.Log("Inject operation completed successfully");
                return 0;
            }
            catch (Exception ex) when (ex is JsonException or FileNotFoundException or ReflectionTypeLoadException or TargetInvocationException)
            {
                Logger.Log("Injection failed with expected error type", ex);
                throw; // 重新抛出特定异常，让上层统一处理
            }
            catch (Exception ex)
            {
                Logger.Log("Injection failed with unexpected error", ex);
                throw new InvalidOperationException("Unexpected error during injection process", ex);
            }
        }

        /// <summary>
        /// 智能获取配置：优先使用参数，参数为空时使用默认路径
        /// </summary>
        private InjectionConfig GetConfiguration(string? configFile)
        {
            // 策略：参数配置 > 默认路径配置
            string actualConfigPath = !string.IsNullOrWhiteSpace(configFile)
                ? ResolveConfigPath(configFile)
                : DefaultConfigPath.Value;

            Logger.Log($"Loading configuration from: {actualConfigPath}");

            if (!File.Exists(actualConfigPath))
            {
                throw new FileNotFoundException($"Configuration file not found: {actualConfigPath}");
            }

            try
            {
                string configContent = File.ReadAllText(actualConfigPath);
                if (string.IsNullOrWhiteSpace(configContent))
                {
                    throw new InvalidOperationException($"Configuration file is empty: {actualConfigPath}");
                }

                var options = new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true,
                    ReadCommentHandling = JsonCommentHandling.Skip
                };

                var config = JsonSerializer.Deserialize<InjectionConfig>(configContent, options);
                return config ?? throw new InvalidOperationException($"Configuration deserialization returned null for file: {actualConfigPath}");
            }
            catch (JsonException jsonEx)
            {
                throw new InvalidOperationException($"Invalid JSON format in configuration file: {actualConfigPath}", jsonEx);
            }
            catch (UnauthorizedAccessException accessEx)
            {
                throw new InvalidOperationException($"Access denied reading configuration file: {actualConfigPath}", accessEx);
            }
        }

        /// <summary>
        /// 解析配置文件路径（支持相对路径和绝对路径）
        /// </summary>
        private static string ResolveConfigPath(string configFile)
        {
            if (Path.IsPathRooted(configFile))
                return configFile;

            var assemblyDir = GetAssemblyDirectory();
            return Path.GetFullPath(Path.Combine(assemblyDir, configFile));
        }

        /// <summary>
        /// 获取默认配置文件路径
        /// </summary>
        private static string GetDefaultConfigPath()
        {
            var assemblyDir = GetAssemblyDirectory();
            return Path.Combine(assemblyDir, "InjectionConfig.json");
        }

        /// <summary>
        /// 获取程序集所在目录
        /// </summary>
        private static string GetAssemblyDirectory()
        {
            var assemblyLocation = typeof(InjectionInterface).Assembly.Location;
            if (string.IsNullOrEmpty(assemblyLocation))
            {
                throw new InvalidOperationException("Unable to determine assembly location");
            }

            var assemblyDir = Path.GetDirectoryName(assemblyLocation);
            return assemblyDir ?? throw new InvalidOperationException($"Unable to determine directory for assembly: {assemblyLocation}");
        }

        /// <summary>
        /// 验证配置的完整性和有效性
        /// </summary>
        private static void ValidateConfiguration(InjectionConfig config)
        {
            if (config == null)
                throw new ArgumentNullException(nameof(config));

            if (string.IsNullOrWhiteSpace(config.TargetAssemblyPath))
                throw new InvalidOperationException("TargetAssemblyPath is required in configuration");

            if (string.IsNullOrWhiteSpace(config.TargetTypeName))
                throw new InvalidOperationException("TargetTypeName is required in configuration");

            if (string.IsNullOrWhiteSpace(config.TargetMethodName))
                throw new InvalidOperationException("TargetMethodName is required in configuration");

            Logger.Log("Configuration validation passed");
        }

        /// <summary>
        /// 加载依赖程序集
        /// </summary>
        private void LoadDependencyAssemblies(List<string>? dependencyPaths)
        {
            if (dependencyPaths == null || dependencyPaths.Count == 0)
            {
                Logger.Log("No dependency assemblies to load");
                return;
            }

            Logger.Log($"Loading {dependencyPaths.Count} dependency assemblies");

            var successfulLoads = new List<string>();
            var failedLoads = new List<(string Path, Exception Error)>();

            foreach (var path in dependencyPaths.Where(p => !string.IsNullOrWhiteSpace(p)))
            {
                try
                {
                    var resolvedPath = ResolveConfigPath(path);
                    Assembly.LoadFrom(resolvedPath);
                    successfulLoads.Add(resolvedPath);
                    Logger.Log($"Successfully loaded dependency: {resolvedPath}");
                }
                catch (Exception ex)
                {
                    failedLoads.Add((path, ex));
                    Logger.Log($"Failed to load dependency assembly: {path}", ex);
                }
            }

            // 如果有任何依赖加载失败，抛出异常
            if (failedLoads.Count > 0)
            {
                var errors = string.Join("; ", failedLoads.Select(f => $"{f.Path}: {f.Error.Message}"));
                throw new InvalidOperationException($"Failed to load {failedLoads.Count} dependency assemblies: {errors}");
            }

            Logger.Log($"All {successfulLoads.Count} dependency assemblies loaded successfully");
        }

        /// <summary>
        /// 执行目标方法
        /// </summary>
        private void ExecuteTargetMethod(InjectionConfig config)
        {
            Logger.Log($"Loading target assembly: {config.TargetAssemblyPath}");

            var assemblyPath = ResolveConfigPath(config.TargetAssemblyPath);
            var assembly = Assembly.LoadFrom(assemblyPath);
            Logger.Log($"Target assembly loaded: {assembly.FullName}");

            var type = assembly.GetType(config.TargetTypeName)
                ?? throw new InvalidOperationException($"Target type '{config.TargetTypeName}' not found in assembly '{assemblyPath}'");

            Logger.Log($"Target type found: {type.FullName}");

            var method = type.GetMethod(config.TargetMethodName)
                ?? throw new InvalidOperationException($"Target method '{config.TargetMethodName}' not found in type '{config.TargetTypeName}'");

            Logger.Log($"Target method found: {method.Name}");

            // 调用目标方法
            Logger.Log($"Invoking {config.TargetTypeName}.{config.TargetMethodName}");
            try
            {
                method.Invoke(null, [config.MethodArguments]);
                Logger.Log("Target method executed successfully");
            }
            catch (TargetInvocationException invokeEx)
            {
                Logger.Log($"Target method execution failed: {invokeEx.InnerException?.Message}", invokeEx);
                throw; // 保留原始堆栈信息
            }
        }

        /// <summary>
        /// 释放资源
        /// </summary>
        public void Dispose()
        {
            if (!_disposed)
            {
                Logger.Log("Disposing InjectionInterface resources");
                _disposed = true;
                GC.SuppressFinalize(this);
            }
        }

        /// <summary>
        /// 析构函数作为安全网
        /// </summary>
        ~InjectionInterface() => Dispose();
    }
}