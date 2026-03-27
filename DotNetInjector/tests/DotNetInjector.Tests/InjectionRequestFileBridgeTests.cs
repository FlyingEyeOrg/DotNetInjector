using System.Text;
using DotNetInjector.Models;
using DotNetInjector.Services;

namespace DotNetInjector.Tests;

internal static class InjectionRequestFileBridgeTests
{
    public static void Run()
    {
        Publish_WritesAndDeletesPrimaryRequestFile();
        Publish_WritesAndDeletesAdditionalRequestFile();
    }

    private static void Publish_WritesAndDeletesPrimaryRequestFile()
    {
        var request = CreateRequest();
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
            AssertRequestFileContents(request_file_path, request);
        }

        AssertEx.False(File.Exists(request_file_path), "request file was not deleted during dispose");
    }

    private static void Publish_WritesAndDeletesAdditionalRequestFile()
    {
        var request = CreateRequest();
        var additional_directory = Path.Combine(Path.GetTempPath(), "DotNetInjector-Tests", Guid.NewGuid().ToString("N"));
        var request_file_paths = InjectionRequestFileBridge.GetRequestFilePaths(request.ProcessId, additional_directory);
        var additional_request_file_path = request_file_paths[1];
        var module_request_file_path = request_file_paths[2];

        using (var bridge = InjectionRequestFileBridge.Publish(request, additional_directory))
        {
            AssertEx.True(bridge.PublishedRequestFilePaths.Count == 3, "request file path count mismatch");
            AssertEx.Equal(additional_request_file_path, bridge.PublishedRequestFilePaths[1], "additional request file path mismatch");
            AssertEx.Equal(module_request_file_path, bridge.PublishedRequestFilePaths[2], "module request file path mismatch");
            AssertEx.True(File.Exists(additional_request_file_path), "additional request file was not created");
            AssertEx.True(File.Exists(module_request_file_path), "module request file was not created");
            AssertRequestFileContents(additional_request_file_path, request);
            AssertRequestFileContents(module_request_file_path, request);
        }

        AssertEx.False(File.Exists(additional_request_file_path), "additional request file was not deleted during dispose");
        AssertEx.False(File.Exists(module_request_file_path), "module request file was not deleted during dispose");
    }

    private static ManagedInjectionRequest CreateRequest()
    {
        return new ManagedInjectionRequest(
            43210,
            InjectionRuntimeKind.DotNet,
            null,
            @"E:\payloads\Sample.dll",
            "Demo.Entry",
            "Run",
            "arg-value");
    }

    private static void AssertRequestFileContents(string requestFilePath, ManagedInjectionRequest request)
    {
        var entries = File.ReadAllLines(requestFilePath, Encoding.UTF8)
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

    private static string Decode(string value)
    {
        return Encoding.UTF8.GetString(Convert.FromBase64String(value));
    }
}