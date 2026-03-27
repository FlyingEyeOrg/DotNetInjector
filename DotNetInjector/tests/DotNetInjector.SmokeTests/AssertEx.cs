namespace DotNetInjector.SmokeTests;

internal static class AssertEx
{
    public static void True(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    public static void Contains(string expectedSubstring, string actualValue, string message)
    {
        if (actualValue.IndexOf(expectedSubstring, StringComparison.OrdinalIgnoreCase) < 0)
        {
            throw new InvalidOperationException($"{message}. Missing substring: '{expectedSubstring}'. Actual: '{actualValue}'.");
        }
    }

    public static void DoesNotContain(string unexpectedSubstring, string actualValue, string message)
    {
        if (actualValue.IndexOf(unexpectedSubstring, StringComparison.OrdinalIgnoreCase) >= 0)
        {
            throw new InvalidOperationException($"{message}. Unexpected substring: '{unexpectedSubstring}'. Actual: '{actualValue}'.");
        }
    }
}