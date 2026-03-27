using System;
using System.Diagnostics;
using System.Reflection;
using System.Text;

namespace InjectionClassLibrary
{
    public class InjectionClass
    {
        public static int InjectionMethod(string value)
        {
            Console.OutputEncoding = Encoding.UTF8;

            Console.WriteLine(Process.GetCurrentProcess().Id);
            AppDomain.CurrentDomain.FirstChanceException += (s, e) =>
            {
                Console.WriteLine("[FirstChance] " + e.Exception.Message);
            };

            AppDomain.CurrentDomain.UnhandledException += (s, e) =>
            {
                Console.WriteLine("[Unhandled] " + (e.ExceptionObject as Exception)?.Message);
            };

            Console.WriteLine("print value: " + value);
            Console.WriteLine("方法强制调用成功");

            var asm = Assembly.GetEntryAssembly();

            Console.WriteLine("Entry Assembly: " + asm?.FullName);

            return 0;
        }
    }
}
