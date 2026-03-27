using System.IO;
using System.Text;
using DotNetInjector.Models;

namespace DotNetInjector.Services;

internal sealed class InjectionRequestFileBridge : IDisposable
{
    private const string k_request_directory_name = "DotNetInjector\\requests";

    private readonly IReadOnlyList<string> request_file_paths_;
    private bool disposed_;

    private InjectionRequestFileBridge(IReadOnlyList<string> requestFilePaths)
    {
        request_file_paths_ = requestFilePaths;
    }

    public string RequestFilePath => request_file_paths_[0];

    public IReadOnlyList<string> PublishedRequestFilePaths => request_file_paths_;

    public static InjectionRequestFileBridge Publish(ManagedInjectionRequest request, params string?[] additionalBaseDirectories)
    {
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

        var request_file_paths = GetRequestFilePaths(request.ProcessId, additionalBaseDirectories);
        foreach (var request_file_path in request_file_paths)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(request_file_path)!);

            var temp_file_path = request_file_path + $".{Environment.ProcessId}.{Guid.NewGuid():N}.tmp";
            File.WriteAllLines(temp_file_path, lines, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            File.Move(temp_file_path, request_file_path, overwrite: true);
        }

        return new InjectionRequestFileBridge(request_file_paths);
    }

    internal static string GetRequestFilePath(int processId, string? baseDirectory = null)
    {
        var request_directory = baseDirectory ?? Path.Combine(Path.GetTempPath(), k_request_directory_name);
        return Path.Combine(request_directory, $"request-{processId}.txt");
    }

    internal static string GetModuleRequestFilePath(string baseDirectory)
    {
        return Path.Combine(baseDirectory, "request.txt");
    }

    internal static IReadOnlyList<string> GetRequestFilePaths(int processId, params string?[] additionalBaseDirectories)
    {
        var paths = new List<string>();
        var seen_paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        AddPath(paths, seen_paths, GetRequestFilePath(processId));

        foreach (var additional_base_directory in additionalBaseDirectories)
        {
            if (string.IsNullOrWhiteSpace(additional_base_directory))
            {
                continue;
            }

            AddPath(paths, seen_paths, GetRequestFilePath(processId, additional_base_directory));
            AddPath(paths, seen_paths, GetModuleRequestFilePath(additional_base_directory));
        }

        return paths;
    }

    public void Dispose()
    {
        if (disposed_)
        {
            return;
        }

        try
        {
            foreach (var request_file_path in request_file_paths_)
            {
                if (!File.Exists(request_file_path))
                {
                    continue;
                }

                File.Delete(request_file_path);
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

    private static void AddPath(ICollection<string> paths, ISet<string> seenPaths, string path)
    {
        if (seenPaths.Add(path))
        {
            paths.Add(path);
        }
    }
}