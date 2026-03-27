using System.Diagnostics;
using System.Runtime.InteropServices;

namespace DotNetInjector.Services;

internal static class ProcessArchitectureHelper
{
    [DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.Winapi)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWow64Process(IntPtr process, out bool wow64Process);

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
}