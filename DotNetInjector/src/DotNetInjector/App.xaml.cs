using Autofac;
using Autofac.Extensions.DependencyInjection;
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
        private IHost? host_;

        public App()
        {
            Startup += App_Startup;
            Exit += App_Exit;
        }

        private void App_Startup(object sender, StartupEventArgs e)
        {
            // 1. 先创建 Serilog 全局日志（可选，主要用于非 DI 场景）
            Log.Logger = new LoggerConfiguration()
                .Enrich.FromLogContext()
#if RELEASE
                .MinimumLevel.Information()
#else
                .MinimumLevel.Debug()
#endif
                .WriteTo.File(
                    path: "logs/app/log-.txt",
                    rollingInterval: RollingInterval.Day,
                    rollOnFileSizeLimit: true,
                    retainedFileCountLimit: 7,
                    outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}",
                    fileSizeLimitBytes: 10 * 1024 * 1024
                )
                .CreateLogger();

            Log.Information("应用程序启动！");

#if RELEASE
            var environmentName = Environments.Production;
#else
            var environmentName = Environments.Development;
#endif

            // 2. 创建 HostBuilder
            var builder = Host.CreateApplicationBuilder(new HostApplicationBuilderSettings
            {
                ContentRootPath = AppContext.BaseDirectory,
                EnvironmentName = environmentName,
            });

            // 3. 先配置 Serilog（关键：必须在 Build 之前）
            builder.Services.AddSerilog();

            // 4. 配置 Autofac
            builder.ConfigureContainer(new AutofacServiceProviderFactory(), autofacBuilder =>
            {
                autofacBuilder.RegisterModule<DotNetInjectorModule>();
            });

            // 5. 配置 appsettings
            builder.Configuration.SetBasePath(AppContext.BaseDirectory)
                .AddJsonFile("appsettings.json", optional: true, reloadOnChange: true)
                .AddJsonFile($"appsettings.{environmentName}.json", optional: true)
                .AddEnvironmentVariables();

            // 6. 构建并启动 Host
            host_ = builder.Build();
            host_.Start();

            var mainWindow = host_.Services.GetRequiredService<MainWindow>();
            mainWindow.Show();
            MainWindow = mainWindow;
        }

        private void App_Exit(object sender, ExitEventArgs e)
        {
            if (host_ is null)
            {
                Log.CloseAndFlush();
                return;
            }

            host_.StopAsync().GetAwaiter().GetResult();
            host_.Dispose();
            Log.CloseAndFlush();
        }
    }
}
