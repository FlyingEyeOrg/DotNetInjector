using System.Diagnostics;
using System.Runtime.InteropServices;

namespace DotNetInjector.Services;

internal static class ProcessArchitectureHelper
{
    private const ushort k_image_file_machine_i386 = 0x014c;

    [DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.Winapi)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWow64Process(IntPtr process, out bool wow64Process);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private delegate bool IsWow64Process2Delegate(IntPtr process, out ushort processMachine, out ushort nativeMachine);

    public static bool TryGetArchitecture(Process process, out string architecture)
    {
        architecture = "unknown";

        try
        {
            if (!Environment.Is64BitOperatingSystem)
            {
                architecture = "x86";
                return true;
            }

            if (TryGetArchitectureViaIsWow64Process2(process.Handle, out architecture))
            {
                return true;
            }

            if (!IsWow64Process(process.Handle, out var isWow64))
            {
                return false;
            }

            architecture = isWow64 ? "x86" : "x64";
            return true;
        }
        catch
        {
            return false;
        }
    }

    private static bool TryGetArchitectureViaIsWow64Process2(IntPtr processHandle, out string architecture)
    {
        architecture = "unknown";

        var kernel32 = GetModuleHandle("kernel32.dll");
        if (kernel32 == IntPtr.Zero)
        {
            return false;
        }

        var export = GetProcAddress(kernel32, "IsWow64Process2");
        if (export == IntPtr.Zero)
        {
            return false;
        }

        var is_wow64_process2 = Marshal.GetDelegateForFunctionPointer<IsWow64Process2Delegate>(export);
        if (!is_wow64_process2(processHandle, out var process_machine, out _))
        {
            return false;
        }

        architecture = process_machine == k_image_file_machine_i386 ? "x86" : "x64";
        return true;
    }
}