using DotNetInjector.Models;
using DotNetInjector.Services;

namespace DotNetInjector.Tests;

internal static class InjectorCommandLineBuilderTests
{
    public static void Run()
    {
        BuildArguments_UsesReadmeCompatibleDefaults();
        BuildArguments_OmitsBackendOutsideRemoteThread();
    }

    private static void BuildArguments_UsesReadmeCompatibleDefaults()
    {
        const string payload_path = @"E:\payloads\ManagedInjectionLibrary.dll";

        var request = new ManagedInjectionRequest(
            1234,
            InjectionRuntimeKind.DotNet,
            null,
            payload_path,
            "Demo.Entry",
            "Run",
            "arg",
            InjectionPayloadKind.LoadLibrary,
            InjectionExecutionKind.RemoteThread,
            RemoteThreadBackendKind.NtCreateThreadEx);

        var arguments = InjectorCommandLineBuilder.BuildArguments(request, payload_path);

        AssertEx.Equal(
            "1234 \"E:\\payloads\\ManagedInjectionLibrary.dll\" --payload=load-library --execution=remote-thread --remote-thread-backend=ntcreatethreadex",
            arguments,
            "injector arguments mismatch");
    }

    private static void BuildArguments_OmitsBackendOutsideRemoteThread()
    {
        const string payload_path = @"E:\payloads\ManagedInjectionLibrary.dll";

        var request = new ManagedInjectionRequest(
            4321,
            InjectionRuntimeKind.DotNet,
            null,
            payload_path,
            "Demo.Entry",
            "Run",
            "arg",
            InjectionPayloadKind.ManualMap,
            InjectionExecutionKind.ThreadContext,
            RemoteThreadBackendKind.Auto);

        var arguments = InjectorCommandLineBuilder.BuildArguments(request, payload_path);

        AssertEx.Equal(
            "4321 \"E:\\payloads\\ManagedInjectionLibrary.dll\" --payload=manual-map --execution=thread-context",
            arguments,
            "injector arguments mismatch");
    }
}