using System.Text;
using DotNetInjector.Models;
using DotNetInjector.Services;

namespace DotNetInjector.Tests;

internal static class InjectionRequestFileBridgeTests
{
    public static void Run()
    {
        var request = new ManagedInjectionRequest(
            43210,
            InjectionRuntimeKind.DotNet,
            null,
            @"E:\payloads\Sample.dll",
            "Demo.Entry",
            "Run",
            "arg-value");

        var request_file_path = InjectionRequestFileBridge.GetRequestFilePath(request.ProcessId);
        Directory.CreateDirectory(Path.GetDirectoryName(request_file_path)!);
        if (File.Exists(request_file_path))
        {
            File.Delete(request_file_path);
        }

        using (var bridge = InjectionRequestFileBridge.Publish(request))
        {
            AssertEx.Equal(request_file_path, bridge.RequestFilePath, "request file path mismatch");
            AssertEx.True(File.Exists(request_file_path), "request file was not created");

            var entries = File.ReadAllLines(request_file_path, Encoding.UTF8)
                .Select(line => line.Split('=', 2))
                .ToDictionary(parts => parts[0], parts => parts.Length > 1 ? parts[1] : string.Empty, StringComparer.Ordinal);

            AssertEx.Equal("1", entries["format_version"], "format_version mismatch");
            AssertEx.Equal(request.ProcessId.ToString(), entries["process_id"], "process_id mismatch");
            AssertEx.Equal(".NetCore", Decode(entries["runtime_hint"]), "runtime hint mismatch");
            AssertEx.Equal(request.AssemblyPath, Decode(entries["assembly_path"]), "assembly path mismatch");
            AssertEx.Equal(request.EntryClass, Decode(entries["entry_class"]), "entry class mismatch");
            AssertEx.Equal(request.EntryMethod, Decode(entries["entry_method"]), "entry method mismatch");
            AssertEx.Equal(request.EntryArgument, Decode(entries["entry_argument"]), "entry argument mismatch");
        }

        AssertEx.False(File.Exists(request_file_path), "request file was not deleted during dispose");
    }

    private static string Decode(string value)
    {
        return Encoding.UTF8.GetString(Convert.FromBase64String(value));
    }
}