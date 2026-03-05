using System;
using System.Runtime.InteropServices;

namespace DotNetInjector.Utils
{
    using DWORD = uint;  // Windows DWORD 是 32 位无符号整数

    public static class SharedMemoryLib
    {
        private const string DllName = "SharedMemoryLib.dll";

        // ========== 共享内存相关 ==========

        /// <summary>
        /// 创建带 Everyone 只读权限的共享内存
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD CreateEmptyReadOnlySharedMemory(
            string pName,
            DWORD dwBufferSize,
            out IntPtr phMapFile);

        /// <summary>
        /// 向已存在的共享内存写入二进制数据
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD WriteToSharedMemory(
            IntPtr hMapFile,
            byte[] pData,
            DWORD dwDataSize);

        /// <summary>
        /// 释放共享内存句柄及相关资源
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD ReleaseSharedMemory(
            ref IntPtr phMapFile);

        // ========== 互斥量相关 ==========

        /// <summary>
        /// 创建带 Everyone 完全访问权限的全局互斥量
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD CreateGlobalMutexWithEveryoneAccess(
            string pName,
            out IntPtr phMutex);

        /// <summary>
        /// 等待互斥量（支持超时）
        /// </summary>
        /// <param name="hMutex">互斥量句柄</param>
        /// <param name="dwMilliseconds">等待超时时间（毫秒），0xFFFFFFFF 表示无限等待</param>
        /// <returns>
        /// ERROR_SUCCESS (0) - 成功获取互斥量
        /// WAIT_TIMEOUT (258) - 超时
        /// WAIT_ABANDONED (128) - 前一个持有者异常退出
        /// 其他错误码 - 失败
        /// </returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD WaitForMutex(
            IntPtr hMutex,
            DWORD dwMilliseconds);

        /// <summary>
        /// 释放互斥量（必须由持有者调用）
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD ReleaseMutexHandle(
            IntPtr hMutex);

        /// <summary>
        /// 关闭互斥量句柄并置空
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern DWORD CloseMutex(
            ref IntPtr phMutex);

        // ========== 常量定义 ==========

        /// <summary>
        /// 无限等待
        /// </summary>
        public const DWORD INFINITE = 0xFFFFFFFF;

        /// <summary>
        /// 等待超时
        /// </summary>
        public const DWORD WAIT_TIMEOUT = 0x00000102;

        /// <summary>
        /// 前一个持有者异常退出
        /// </summary>
        public const DWORD WAIT_ABANDONED = 0x00000080;

        /// <summary>
        /// 操作成功
        /// </summary>
        public const DWORD ERROR_SUCCESS = 0;
    }
}