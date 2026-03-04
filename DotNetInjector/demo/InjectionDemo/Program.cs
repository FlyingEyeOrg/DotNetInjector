using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace InjectionDemo
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
