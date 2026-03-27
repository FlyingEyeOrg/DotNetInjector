using Autofac;
using DotNetInjector.Services;
using DotNetInjector.ViewModel;

namespace DotNetInjector
{
    internal class DotNetInjectorModule : Module
    {
        public DotNetInjectorModule()
        {
        }

        protected override void Load(ContainerBuilder builder)
        {
            base.Load(builder);

            builder.RegisterType<ProcessCatalogService>()
                .As<IProcessCatalogService>()
                .SingleInstance();
            builder.RegisterType<ManagedInjectorService>()
                .As<IManagedInjectorService>()
                .SingleInstance();
            builder.RegisterType<MainWindowViewModel>()
               .AsSelf()
               .SingleInstance();
            builder.RegisterType<MainWindow>()
                .AsSelf()
                .OnActivated(e => e.Instance.DataContext = e.Context.Resolve<MainWindowViewModel>());
        }
    }
}
