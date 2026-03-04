using System.Diagnostics;

namespace CoreInjectionDemo
{
    internal class Program
    {
        static void Main(string[] args)
        {
            if (Environment.Is64BitProcess)
            {
                Console.WriteLine("arc: x64");
            }
            else
            {
                Console.WriteLine("arc: x86");
            }

            Console.WriteLine("process id: " + Process.GetCurrentProcess().Id);
            Console.WriteLine("enter any ke to exit...");
            Console.ReadKey();
        }
    }
}
