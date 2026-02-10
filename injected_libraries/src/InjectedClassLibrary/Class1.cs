using System;
using System.IO;
using System.Reflection;

namespace InjectedClassLibrary
{
    public class InjectedInterface
    {
        public static int Inject(string args)
        {
            //Path.Combine(AppContext.BaseDirectory, "")
            Assembly.LoadFrom("");

            return 0;
        }
    }
}
