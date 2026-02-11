using System;

namespace InjectedClassLibrary
{
    public class InjectionInterface : IDisposable
    {
        private bool _disposed = false;

        // 私有构造函数，防止外部实例化
        private InjectionInterface()
        {
        }

        /// <summary>
        /// 静态方法 - 适合一次性调用的场景
        /// </summary>
        public static int Inject(string args)
        {
            InjectionInterface injector;

            try
            {
                // 不要在 using 中使用，否则会立即释放资源
                injector = new InjectionInterface();
            }
            catch (Exception)
            {
                return -1;
            }

            try
            {
                return injector.InjectInternal(args);
            }
            catch (Exception ex)
            {
                // 记录日志（如果可能）
                Logger.Log("Inject operation failed", ex);
                return -1;
            }
            finally
            {
                // 手动释放资源
                injector.Dispose();
            }
        }

        private int InjectInternal(string args)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(InjectionInterface));

            Logger.Log($"Inject method called with args: {args}");
            Logger.Log($"Base directory: {AppContext.BaseDirectory}");

            // 注入逻辑
            // ...

            Logger.Log("Inject operation completed successfully");
            return 0;
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _disposed = true;
            }
        }
    }
}