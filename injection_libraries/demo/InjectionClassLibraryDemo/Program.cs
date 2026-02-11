using InjectedClassLibrary;

namespace InjectionClassLibraryDemo
{
    internal class Program
    {
        static void Main(string[] args)
        {
            InjectionInterface.Inject("hello");
        }
    }
}
