namespace DotNetInjector.Tests;

internal static class AssertEx
{
    public static void True(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    public static void False(bool condition, string message)
    {
        True(!condition, message);
    }

    public static void Equal(string? expected, string? actual, string message)
    {
        if (!string.Equals(expected, actual, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"{message}. Expected: '{expected}', Actual: '{actual}'.");
        }
    }

    public static void Contains(string expectedSubstring, string actualValue, string message)
    {
        if (actualValue.IndexOf(expectedSubstring, StringComparison.OrdinalIgnoreCase) < 0)
        {
            throw new InvalidOperationException($"{message}. Missing substring: '{expectedSubstring}'. Actual: '{actualValue}'.");
        }
    }
}