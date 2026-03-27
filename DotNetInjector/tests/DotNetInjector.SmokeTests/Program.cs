namespace DotNetInjector.SmokeTests;

internal static class Program
{
    public static async Task<int> Main()
    {
        try
        {
            await InjectionSmokeTests.RunAsync();
            Console.WriteLine("[PASS] DotNetInjector.SmokeTests");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 1;
        }
    }
}