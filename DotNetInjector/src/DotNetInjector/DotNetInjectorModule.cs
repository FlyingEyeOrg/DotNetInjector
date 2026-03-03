using Autofac;
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

            builder.RegisterType<MainWindowViewModel>()
               .AsSelf();
            builder.RegisterType<MainWindow>()
                .AsSelf()
                .OnActivated(e => e.Instance.DataContext = e.Context.Resolve<MainWindowViewModel>());
        }
    }
}
