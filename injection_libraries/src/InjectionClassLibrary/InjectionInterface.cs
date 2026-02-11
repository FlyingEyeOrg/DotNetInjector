using System;
using System.IO;
using System.Reflection;
using System.Text.Json;

namespace InjectedClassLibrary
{
    /// <summary>
    /// 注入接口类 - 提供动态程序集加载和方法调用功能
    /// 设计为单次使用的工具类，使用后自动清理资源
    /// </summary>
    public class InjectionInterface : IDisposable
    {
        private bool _disposed = false;

        /// <summary>
        /// 私有构造函数，强制使用静态方法进行实例化
        /// 确保资源管理的统一性和正确性
        /// </summary>
        private InjectionInterface()
        {
            Logger.Log("InjectionInterface instance created");
        }

        /// <summary>
        /// 静态注入方法 - 适合一次性调用的场景
        /// 自动管理对象生命周期，无需手动释放资源
        /// </summary>
        /// <param name="args">注入参数（当前未使用，保留扩展）</param>
        /// <returns>执行结果：0=成功，负数=失败</returns>
        public static int Inject(string args)
        {
            Logger.Log($"Static Inject method called with args: '{args}'");

            InjectionInterface? injector = null;
            try
            {
                // 创建实例但不使用using，避免立即释放
                injector = new InjectionInterface();
                Logger.Log("InjectionInterface instance initialized successfully");

                return injector.InjectInternal(args);
            }
            catch (Exception ex)
            {
                // 记录详细的错误信息
                Logger.Log($"Inject operation failed - Args: '{args}'", ex);
                return -1;
            }
            finally
            {
                // 手动释放资源，确保即使出现异常也能正确清理
                if (injector != null)
                {
                    try
                    {
                        injector.Dispose();
                        Logger.Log("InjectionInterface instance disposed successfully");
                    }
                    catch (Exception disposeEx)
                    {
                        Logger.Log("Failed to dispose InjectionInterface instance", disposeEx);
                    }
                }
            }
        }

        /// <summary>
        /// 内部注入实现方法 - 核心业务逻辑
        /// </summary>
        /// <param name="args">注入参数</param>
        /// <returns>执行状态码</returns>
        private int InjectInternal(string args)
        {
            Logger.Log("Starting internal injection process");

            // 检查对象状态，防止重复调用导致的状态不一致
            if (_disposed)
            {
                string errorMsg = "Cannot execute InjectInternal on disposed InjectionInterface instance";
                Logger.Log(errorMsg);
                throw new ObjectDisposedException(nameof(InjectionInterface), errorMsg);
            }

            try
            {
                Logger.Log($"Processing injection with arguments: {args ?? "null"}");
                Logger.Log($"Application base directory: {AppContext.BaseDirectory}");
                Logger.Log($"Current assembly location: {typeof(InjectionInterface).Assembly.Location}");

                // 获取并验证配置
                var config = GetConfiguration();
                ValidateConfiguration(config);

                // 优先加载依赖的程序集
                LoadDependencyAssemblies(config.DependencyAssemblyPaths.ToArray());

                // 加载目标程序集并执行方法
                ExecuteTargetMethod(config);

                Logger.Log("Inject operation completed successfully");
                return 0;
            }
            catch (JsonException jsonEx)
            {
                string errorMsg = $"Configuration file parsing failed - invalid JSON format";
                Logger.Log(errorMsg, jsonEx);
                throw new InvalidOperationException(errorMsg, jsonEx);
            }
            catch (FileNotFoundException fileEx)
            {
                string errorMsg = $"Required file not found during injection process";
                Logger.Log(errorMsg, fileEx);
                throw new InvalidOperationException(errorMsg, fileEx);
            }
            catch (ReflectionTypeLoadException loadEx)
            {
                string errorMsg = $"Type loading failed during assembly reflection";
                Logger.Log($"{errorMsg} - Loader exceptions count: {loadEx.LoaderExceptions?.Length ?? 0}", loadEx);

                // 记录所有加载异常
                if (loadEx.LoaderExceptions != null)
                {
                    for (int i = 0; i < loadEx.LoaderExceptions.Length; i++)
                    {
                        var tmpEx = loadEx.LoaderExceptions[i];
                        if (tmpEx != null)
                            Logger.Log($"Loader exception {i + 1}", tmpEx);
                    }
                }
                throw new InvalidOperationException(errorMsg, loadEx);
            }
            catch (TargetInvocationException invokeEx)
            {
                string errorMsg = $"Target method invocation failed";
                Logger.Log(errorMsg, invokeEx);
                throw new InvalidOperationException($"{errorMsg} - Inner exception: {invokeEx.InnerException?.Message}", invokeEx);
            }
            catch (Exception ex)
            {
                string errorMsg = $"Unexpected error during injection process";
                Logger.Log(errorMsg, ex);
                throw new InvalidOperationException(errorMsg, ex);
            }
        }

        /// <summary>
        /// 验证配置的完整性和有效性
        /// </summary>
        /// <param name="config">配置对象</param>
        private void ValidateConfiguration(InjectionConfig config)
        {
            if (config == null)
            {
                throw new ArgumentNullException(nameof(config), "Configuration cannot be null");
            }

            if (string.IsNullOrWhiteSpace(config.TargetAssemblyPath))
            {
                throw new InvalidOperationException("TargetAssemblyPath is required in configuration");
            }

            if (string.IsNullOrWhiteSpace(config.TargetTypeName))
            {
                throw new InvalidOperationException("TargetTypeName is required in configuration");
            }

            if (string.IsNullOrWhiteSpace(config.TargetMethodName))
            {
                throw new InvalidOperationException("TargetMethodName is required in configuration");
            }

            Logger.Log("Configuration validation passed");
        }

        /// <summary>
        /// 加载依赖程序集
        /// </summary>
        /// <param name="dependencyPaths">依赖程序集路径列表</param>
        private void LoadDependencyAssemblies(string[] dependencyPaths)
        {
            if (dependencyPaths == null || dependencyPaths.Length == 0)
            {
                Logger.Log("No dependency assemblies to load");
                return;
            }

            Logger.Log($"Loading {dependencyPaths.Length} dependency assemblies");

            foreach (var path in dependencyPaths)
            {
                try
                {
                    if (string.IsNullOrWhiteSpace(path))
                    {
                        Logger.Log("Warning: Empty dependency path encountered, skipping");
                        continue;
                    }

                    Logger.Log($"Loading dependency assembly from: {path}");
                    Assembly.LoadFrom(path);
                    Logger.Log($"Successfully loaded dependency: {path}");
                }
                catch (Exception ex)
                {
                    string errorMsg = $"Failed to load dependency assembly: {path}";
                    Logger.Log(errorMsg, ex);
                    throw new InvalidOperationException(errorMsg, ex);
                }
            }
        }

        /// <summary>
        /// 执行目标方法
        /// </summary>
        /// <param name="config">配置信息</param>
        private void ExecuteTargetMethod(InjectionConfig config)
        {
            Logger.Log($"Loading target assembly: {config.TargetAssemblyPath}");

            // 加载目标程序集
            var assembly = Assembly.LoadFrom(config.TargetAssemblyPath);
            Logger.Log($"Target assembly loaded successfully: {assembly.FullName}");

            // 获取目标类型
            var type = assembly.GetType(config.TargetTypeName);
            if (type == null)
            {
                throw new InvalidOperationException($"Target type '{config.TargetTypeName}' not found in assembly '{config.TargetAssemblyPath}'");
            }
            Logger.Log($"Target type found: {type.FullName}");

            // 获取目标方法
            var method = type.GetMethod(config.TargetMethodName);
            if (method == null)
            {
                throw new InvalidOperationException($"Target method '{config.TargetMethodName}' not found in type '{config.TargetTypeName}'");
            }
            Logger.Log($"Target method found: {method.Name}");

            // 调用目标方法
            Logger.Log($"Invoking target method: {config.TargetTypeName}.{config.TargetMethodName}");
            try
            {
                method.Invoke(null, null);
                Logger.Log("Target method executed successfully");
            }
            catch (Exception ex)
            {
                string errorMsg = $"Target method execution failed: {config.TargetTypeName}.{config.TargetMethodName}";
                Logger.Log(errorMsg, ex);
                throw; // 重新抛出以让上层处理
            }
        }

        /// <summary>
        /// 从配置文件获取注入配置
        /// </summary>
        /// <returns>配置对象</returns>
        private InjectionConfig GetConfiguration()
        {
            Logger.Log("Reading injection configuration");

            var assemblyLocation = typeof(InjectionInterface).Assembly.Location;
            if (string.IsNullOrEmpty(assemblyLocation))
            {
                throw new InvalidOperationException("Unable to determine assembly location for configuration file lookup");
            }

            var assemblyDir = Path.GetDirectoryName(assemblyLocation);
            if (string.IsNullOrEmpty(assemblyDir))
            {
                throw new InvalidOperationException($"Unable to determine directory for assembly location: {assemblyLocation}");
            }

            var configFile = Path.Combine(assemblyDir, "InjectionConfig.json");
            Logger.Log($"Looking for configuration file at: {configFile}");

            if (!File.Exists(configFile))
            {
                throw new FileNotFoundException($"Configuration file not found: {configFile}");
            }

            try
            {
                string configContent = File.ReadAllText(configFile);
                if (string.IsNullOrWhiteSpace(configContent))
                {
                    throw new InvalidOperationException($"Configuration file is empty: {configFile}");
                }

                var config = JsonSerializer.Deserialize<InjectionConfig>(configContent);
                if (config == null)
                {
                    throw new InvalidOperationException($"Configuration deserialization returned null for file: {configFile}");
                }

                Logger.Log("Configuration loaded and parsed successfully");
                return config;
            }
            catch (JsonException jsonEx)
            {
                throw new InvalidOperationException($"Invalid JSON format in configuration file: {configFile}", jsonEx);
            }
            catch (UnauthorizedAccessException accessEx)
            {
                throw new InvalidOperationException($"Access denied reading configuration file: {configFile}", accessEx);
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
                // 这里可以添加其他需要清理的资源
                GC.SuppressFinalize(this);
            }
        }
    }
}