using System;
using System.IO;
using System.Text;

namespace InjectedClassLibrary
{
    internal static class Logger
    {
        private const string LibraryID = "1665a62f-1f65-4857-b720-6f9216ae5054";
        private static readonly string LogFile;
        private static readonly object _lockObj = new object();

        static Logger()
        {
            string logDir = Path.Combine(AppContext.BaseDirectory, $"InjectionLogs[{LibraryID}]");

            // 确保目录存在
            Directory.CreateDirectory(logDir);

            LogFile = Path.Combine(logDir, "injection.log");
        }

        public static void Log(string message)
        {
            try
            {
                string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} - {message}{Environment.NewLine}";

                lock (_lockObj)
                {
                    File.AppendAllText(LogFile, logEntry, Encoding.UTF8);
                }
            }
            catch
            {
                // 忽略日志写入错误
            }
        }

        public static void Log(string message, Exception ex)
        {
            try
            {
                StringBuilder logBuilder = new StringBuilder();
                logBuilder.AppendLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss} - {message}");
                logBuilder.AppendLine($"异常类型: {ex.GetType().Name}");
                logBuilder.AppendLine($"异常信息: {ex.Message}");

                if (!string.IsNullOrEmpty(ex.StackTrace))
                {
                    logBuilder.AppendLine($"堆栈跟踪:");
                    logBuilder.AppendLine(ex.StackTrace);
                }

                // 递归记录内部异常
                Exception? innerEx = ex.InnerException;
                int innerCount = 1;

                while (innerEx != null)
                {
                    logBuilder.AppendLine($"内部异常 {innerCount}: {innerEx.GetType().Name}");
                    logBuilder.AppendLine($"内部异常信息: {innerEx.Message}");
                    if (!string.IsNullOrEmpty(innerEx.StackTrace))
                    {
                        logBuilder.AppendLine($"内部异常堆栈:");
                        logBuilder.AppendLine(innerEx.StackTrace);
                    }
                    innerEx = innerEx.InnerException;
                    innerCount++;
                }

                logBuilder.AppendLine(); // 空行分隔

                lock (_lockObj)
                {
                    File.AppendAllText(LogFile, logBuilder.ToString(), Encoding.UTF8);
                }
            }
            catch
            {
                // 忽略日志写入错误
            }
        }


        public static void Log(string format, params object[] args)
        {
            Log(string.Format(format, args));
        }
    }
}