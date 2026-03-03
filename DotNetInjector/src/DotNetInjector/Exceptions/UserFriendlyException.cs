using Microsoft.Extensions.Logging;

namespace DotNetInjector.Exceptions
{
    /// <summary>
    /// This exception type is directly shown to the user.
    /// </summary>
    public class UserFriendlyException : BusinessException
    {
        public UserFriendlyException(
            string message,
            string? code = null,
            string? details = null,
            Exception? innerException = null,
            LogLevel logLevel = LogLevel.Warning)
            : base(
                  code,
                  message,
                  details,
                  innerException,
                  logLevel)
        {
            Details = details;
        }
    }
}
