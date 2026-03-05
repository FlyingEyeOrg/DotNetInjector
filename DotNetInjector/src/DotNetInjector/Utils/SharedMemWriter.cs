using System;
using System.Text;
using System.Threading;

namespace DotNetInjector.Utils
{
    public class SharedMemWriter : IDisposable
    {
        private ulong _size;
        private string? _name;
        private bool _disposed = false;
        private readonly string AppID = "b62658dd-18f4-4de3-a09c-53c6c6cbf7d4";
        IntPtr _handle = IntPtr.Zero;

        // 互斥量句柄（使用 P/Invoke 管理）
        private IntPtr _mutexHandle = IntPtr.Zero;

        /// <summary>
        /// 共享内存名称
        /// </summary>
        public string? Name => _name;

        /// <summary>
        /// 共享内存大小（字节）
        /// </summary>
        public ulong Size => _size;

        /// <summary>
        /// 创建或打开共享内存
        /// </summary>
        /// <param name="baseName">共享内存名称</param>
        /// <param name="size">共享内存大小（字节）</param>
        public void Create(string baseName, ulong size)
        {
            if (string.IsNullOrEmpty(baseName))
                throw new ArgumentNullException(nameof(baseName), "共享内存名称不能为空。");

            if (size <= 0)
                throw new ArgumentOutOfRangeException(nameof(size), "共享内存大小必须大于 0。");

            _name = baseName;
            _size = size;

            try
            {
                // 检查是否有残留句柄，如果有则直接抛出异常
                if (_handle != IntPtr.Zero)
                {
                    throw new InvalidOperationException(
                        $"共享内存句柄残留，可能存在资源泄漏或未正确释放。请检查之前的 Create/Close 调用。");
                }

                // 创建共享内存
                var code = SharedMemoryLib.CreateEmptyReadOnlySharedMemory(
                    $"Global\\[{AppID}]-{baseName}",
                    (uint)size,
                    out _handle);

                if (code != 0)
                {
                    throw new InvalidOperationException(
                        $"创建共享内存 '{baseName}' 失败。错误码: 0x{code:X8}");
                }

                if (_handle == IntPtr.Zero)
                {
                    throw new InvalidOperationException(
                        $"创建共享内存 '{baseName}' 失败，句柄为空。");
                }

                // 创建带 Everyone 权限的全局互斥量（P/Invoke）
                string mutexName = $"Global\\[{AppID}]-ProcessInjector_SharedMemory_Mutex_{baseName}";
                code = SharedMemoryLib.CreateGlobalMutexWithEveryoneAccess(mutexName, out _mutexHandle);

                if (code != 0 || _mutexHandle == IntPtr.Zero)
                {
                    throw new InvalidOperationException(
                        $"创建互斥量 '{baseName}' 失败。错误码: 0x{code:X8}");
                }
            }
            catch (Exception ex)
            {
                // 清理已分配的资源
                if (_mutexHandle != IntPtr.Zero)
                {
                    SharedMemoryLib.CloseMutex(ref _mutexHandle);
                    _mutexHandle = IntPtr.Zero;
                }

                if (_handle != IntPtr.Zero)
                {
                    SharedMemoryLib.ReleaseSharedMemory(ref _handle);
                    _handle = IntPtr.Zero;
                }

                throw new InvalidOperationException(
                    $"创建共享内存 '{baseName}' 失败。详情: {ex.Message}", ex);
            }
        }

        /// <summary>
        /// 向共享内存写入字符串（UTF-16 编码，与 C++ wchar_t* 兼容，线程/进程安全）
        /// </summary>
        public void Write(string data)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(SharedMemWriter), "共享内存写入器已被释放。");

            if (_handle == IntPtr.Zero)
                throw new InvalidOperationException("共享内存句柄无效，请先调用 Create 方法。");

            SafeWrite(() =>
            {
                // 转成 UTF-16 字节数组，并手动添加 \0\0 终止符
                byte[] bytes;
                if (string.IsNullOrEmpty(data))
                {
                    // 空字符串只写入 \0\0
                    bytes = new byte[sizeof(char)]; // 2 字节
                }
                else
                {
                    // 获取字符串的 UTF-16 字节 + 2 字节终止符
                    byte[] strBytes = Encoding.Unicode.GetBytes(data);
                    bytes = new byte[strBytes.Length + sizeof(char)]; // 多留 2 字节给 \0\0
                    Buffer.BlockCopy(strBytes, 0, bytes, 0, strBytes.Length);
                    // 最后两个字节默认为 0，即 \0\0
                }

                ulong byteCount = (ulong)bytes.Length;

                // 检查空间
                if (byteCount > _size)
                    throw new InvalidOperationException(
                        $"字符串太长，超出共享内存容量。字符串长度: {byteCount} 字节，可用空间: {_size} 字节。");

                // 一次性写入全部数据（包含终止符）
                SharedMemoryLib.WriteToSharedMemory(_handle, bytes, (uint)byteCount);
            });
        }

        /// <summary>
        /// 带互斥量的安全写入
        /// </summary>
        private void SafeWrite(Action writeAction)
        {
            if (_mutexHandle == IntPtr.Zero)
                throw new InvalidOperationException("互斥量句柄无效，可能共享内存创建失败。");

            // 等待互斥量（无限等待）
            var result = SharedMemoryLib.WaitForMutex(_mutexHandle, SharedMemoryLib.INFINITE);

            if (result == SharedMemoryLib.WAIT_ABANDONED)
            {
                // 前一个持有者异常退出，但仍获得锁，继续操作
            }
            else if (result != SharedMemoryLib.ERROR_SUCCESS)
            {
                throw new InvalidOperationException($"等待互斥量失败。错误码: 0x{result:X8}");
            }

            try
            {
                writeAction();
            }
            finally
            {
                // 释放互斥量
                SharedMemoryLib.ReleaseMutexHandle(_mutexHandle);
            }
        }

        /// <summary>
        /// 释放资源
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (_disposed)
                return;

            // 释放互斥量句柄
            if (_mutexHandle != IntPtr.Zero)
            {
                SharedMemoryLib.CloseMutex(ref _mutexHandle);
                _mutexHandle = IntPtr.Zero;
            }

            // 释放共享内存句柄
            if (_handle != IntPtr.Zero)
            {
                SharedMemoryLib.ReleaseSharedMemory(ref _handle);
                _handle = IntPtr.Zero;
            }

            _disposed = true;
        }

        ~SharedMemWriter()
        {
            Dispose(false);
        }
    }
}