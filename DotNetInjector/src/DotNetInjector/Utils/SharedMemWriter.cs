using System;
using System.IO.MemoryMappedFiles;
using System.Text;
using System.Threading;

namespace DotNetInjector.Utils
{
    public class SharedMemWriter : IDisposable
    {
        private MemoryMappedFile? _mmf;
        private MemoryMappedViewAccessor? _accessor;
        private long _size;
        private string? _name;
        private bool _disposed = false;
        private readonly string AppID = "b62658dd-18f4-4de3-a09c-53c6c6cbf7d4";

        // 互斥量，与 C++ 命名保持一致
        private Mutex? _mutex;

        /// <summary>
        /// 共享内存名称
        /// </summary>
        public string? Name => _name;

        /// <summary>
        /// 共享内存大小（字节）
        /// </summary>
        public long Size => _size;

        /// <summary>
        /// 创建或打开共享内存
        /// </summary>
        /// <param name="baseName">共享内存名称</param>
        /// <param name="size">共享内存大小（字节）</param>
        public void Create(string baseName, long size)
        {
            if (string.IsNullOrEmpty(baseName))
                throw new ArgumentNullException(nameof(baseName), "共享内存名称不能为空。");

            if (size <= 0)
                throw new ArgumentOutOfRangeException(nameof(size), "共享内存大小必须大于 0。");

            if (_mmf != null)
                throw new InvalidOperationException("共享内存已经创建，不能重复创建。");

            _name = baseName;
            _size = size;

            try
            {
                // 创建或打开共享内存
                _mmf = MemoryMappedFile.CreateOrOpen($"{AppID}-{baseName}", size, MemoryMappedFileAccess.ReadWrite);
                // 创建访问器
                _accessor = _mmf.CreateViewAccessor(0, size, MemoryMappedFileAccess.ReadWrite);

                // 创建命名互斥量，与 C++ 一致
                string mutexName = $"Global\\[{AppID}]-ProcessInjector_SharedMemory_Mutex_{baseName}";
                _mutex = new Mutex(false, mutexName);
            }
            catch (Exception ex)
            {
                // 发生异常时清理已分配的资源
                _accessor?.Dispose();
                _accessor = null;
                _mmf?.Dispose();
                _mmf = null;
                _mutex?.Dispose();
                _mutex = null;
                throw new InvalidOperationException($"创建共享内存 '{baseName}' 失败。", ex);
            }
        }

        /// <summary>
        /// 向共享内存写入字符串（UTF-8 编码，线程/进程安全）
        /// </summary>
        /// <param name="data">要写入的字符串</param>
        public void Write(string data)
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(SharedMemWriter), "共享内存写入器已被释放。");

            if (_accessor == null)
                throw new InvalidOperationException("共享内存尚未创建，请先调用 Create 方法。");

            if (string.IsNullOrEmpty(data))
            {
                // 写入空字符串，只需写入终止符
                SafeWrite(() => _accessor.Write(0, (byte)0));
                return;
            }

            byte[] bytes = Encoding.UTF8.GetBytes(data);
            int length = Math.Min(bytes.Length, (int)_size - 1); // 留一个字节给 '\0'

            if (length <= 0)
                return; // 空间不够存任何字符（除了终止符）

            SafeWrite(() =>
            {
                _accessor.WriteArray(0, bytes, 0, length);
                _accessor.Write(length, (byte)0); // 手动加个 \0 结尾
            });
        }

        /// <summary>
        /// 带互斥量的安全写入
        /// </summary>
        private void SafeWrite(Action writeAction)
        {
            if (_mutex == null)
                throw new InvalidOperationException("互斥量未初始化。");

            _mutex.WaitOne();
            try
            {
                writeAction();
            }
            finally
            {
                _mutex.ReleaseMutex();
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

            if (disposing)
            {
                // 释放托管资源
                _accessor?.Dispose();
                _accessor = null;

                _mmf?.Dispose();
                _mmf = null;

                _mutex?.Dispose();
                _mutex = null;
            }

            _disposed = true;
        }

        ~SharedMemWriter()
        {
            Dispose(false);
        }
    }
}