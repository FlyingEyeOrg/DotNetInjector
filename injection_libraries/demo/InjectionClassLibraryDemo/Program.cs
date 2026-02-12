using InjectedClassLibrary;
using System.Diagnostics;

namespace InjectionClassLibraryDemo
{
    internal class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine(Process.GetCurrentProcess().Id);
            InjectionInterface.Inject();
            Console.ReadLine();
        }
    }
}
