using System.Diagnostics;
using System.Text;
using System.Threading;

namespace CoreInjectionDemo
{
    internal class Program
    {
        static void Main(string[] args)
        {
            Console.OutputEncoding = Encoding.UTF8;
            Console.InputEncoding = Encoding.UTF8;

            var nonInteractive = Array.Exists(args, static arg =>
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
