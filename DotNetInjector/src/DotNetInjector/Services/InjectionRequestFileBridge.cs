using System.IO;
using System.Text;
using DotNetInjector.Models;

namespace DotNetInjector.Services;

internal sealed class InjectionRequestFileBridge : IDisposable
{
    private const string k_request_directory_name = "DotNetInjector\\requests";

    private readonly string request_file_path_;
    private bool disposed_;

    private InjectionRequestFileBridge(string requestFilePath)
    {
        request_file_path_ = requestFilePath;
    }

    public string RequestFilePath => request_file_path_;

    public static InjectionRequestFileBridge Publish(ManagedInjectionRequest request)
    {
        var request_file_path = GetRequestFilePath(request.ProcessId);
        Directory.CreateDirectory(Path.GetDirectoryName(request_file_path)!);

        var temp_file_path = request_file_path + $".{Environment.ProcessId}.{Guid.NewGuid():N}.tmp";
        var lines = new[]
        {
            "format_version=1",
            $"process_id={request.ProcessId}",
            $"runtime_hint={EncodeValue(GetRuntimeHint(request))}",
            $"assembly_path={EncodeValue(request.AssemblyPath)}",
            $"entry_class={EncodeValue(request.EntryClass)}",
            $"entry_method={EncodeValue(request.EntryMethod)}",
            $"entry_argument={EncodeValue(request.EntryArgument)}",
        };

        File.WriteAllLines(temp_file_path, lines, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        File.Move(temp_file_path, request_file_path, overwrite: true);

        return new InjectionRequestFileBridge(request_file_path);
    }

    internal static string GetRequestFilePath(int processId, string? baseDirectory = null)
    {
        var request_directory = baseDirectory ?? Path.Combine(Path.GetTempPath(), k_request_directory_name);
        return Path.Combine(request_directory, $"request-{processId}.txt");
    }

    public void Dispose()
    {
        if (disposed_)
        {
            return;
        }

        try
        {
            if (File.Exists(request_file_path_))
            {
                File.Delete(request_file_path_);
            }
        }
        catch
        {
        }
        finally
        {
            disposed_ = true;
        }
    }

    private static string EncodeValue(string? value)
    {
        return Convert.ToBase64String(Encoding.UTF8.GetBytes(value ?? string.Empty));
    }

    private static string GetRuntimeHint(ManagedInjectionRequest request)
    {
        return request.Runtime switch
        {
            InjectionRuntimeKind.DotNet => ".NetCore",
            InjectionRuntimeKind.Mono => "Mono",
            _ => request.FrameworkVersion ?? string.Empty,
        };
    }
}