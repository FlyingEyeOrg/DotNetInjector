using System.Configuration;
using System.Data;
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
            var str = Guid.NewGuid().ToString();
        }
    }

}
