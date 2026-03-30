namespace DotNetInjector.Tests;

internal static class Program
{
    public static int Main()
    {
        try
        {
            InjectionRequestFileBridgeTests.Run();
            InjectorCommandLineBuilderTests.Run();
            InjectorAssetResolverTests.Run();
            Console.WriteLine("[PASS] DotNetInjector.Tests");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 1;
        }
    }
}