using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace InjectionClassLibrary
{
    public class InjectionClass
    {
        public static int InjectionMethod(string value)
        {
            Console.WriteLine("print value: " + value);
            Console.WriteLine("方法强制调用成功");

            var asm = Assembly.GetEntryAssembly();

            Console.WriteLine("Entry Assembly: " + asm?.FullName);

            return 0;
        }
    }
}
