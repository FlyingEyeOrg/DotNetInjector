using Autofac;
using Autofac.Extensions.DependencyInjection;
using DotNetInjector.ViewModel;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Serilog;
using System.Windows;

namespace DotNetInjector
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        public App()
        {
            Startup += App_Startup;
        }

        private void App_Startup(object sender, StartupEventArgs e)
        {
            Log.Logger = new LoggerConfiguration()
            .Enrich.FromLogContext()
            #if RELEASE
            .MinimumLevel.Information()
            #else
            .MinimumLevel.Debug()
            #endif
            .WriteTo.File(
                  path: "logs/app/log-.txt", // 注意路径和文件名格式
                  rollingInterval: RollingInterval.Day, // 按天滚动
                  rollOnFileSizeLimit: true, // 在文件大小达到限制时也滚动
                  retainedFileCountLimit: 7, // 保留最近7天的日志文件
                  outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}",
                  fileSizeLimitBytes: 10 * 1024 * 1024 // 每个文件最大10MB
            )
            .CreateLogger();

            Log.Logger.Information("应用程序启动！");
#if RELEASE
            var environmentName = Microsoft.Extensions.Hosting.Environments.Production;
#else
            var environmentName = Microsoft.Extensions.Hosting.Environments.Development;
#endif
            var builder = Host.CreateApplicationBuilder();
            builder.ConfigureContainer(new AutofacServiceProviderFactory(), builder =>
            {
                builder.RegisterModule<DotNetInjectorModule>();
            });
            builder.Configuration.SetBasePath(AppContext.BaseDirectory)
            .AddJsonFile("appsettings.json", optional: true, reloadOnChange: true)
            .AddJsonFile($"appsettings.{environmentName}.json", optional: true)
            .AddEnvironmentVariables();
            builder.Services.AddSerilog();

            var host = builder.Build();
            host.Start();
            var mainWindow = host.Services.GetRequiredService<MainWindow>();
            mainWindow.Show();
            this.MainWindow = mainWindow;
        }
    }
}
