using System;
using System.Diagnostics;
using System.Threading;

namespace InjectionDemo
{
    internal class Program
    {
        static void Main(string[] args)
        {
            var nonInteractive = Array.Exists(args, arg =>
                string.Equals(arg, "--non-interactive", StringComparison.OrdinalIgnoreCase));

            if (Environment.Is64BitProcess)
            {
                Console.WriteLine("arc: x64");
            }
            else
            {
                Console.WriteLine("arc: x86");
            }

            Console.WriteLine("process id: " + Process.GetCurrentProcess().Id);

            if (nonInteractive)
            {
                Console.WriteLine("running in non-interactive mode");
                Thread.Sleep(Timeout.Infinite);
                return;
            }

            Console.WriteLine("press enter to exit...");
            Console.ReadLine();
        }
    }
}
