using System.Collections.Generic;

namespace InjectedClassLibrary
{
    internal class InjectionConfig
    {
        public InjectionConfig(
            string targetAssemblyPath,
            string targetTypeName,
            string targetMethodName,
            string methodArguments,
            List<string> dependencyAssemblyPaths)
        {
            TargetAssemblyPath = targetAssemblyPath;
            TargetTypeName = targetTypeName;
            TargetMethodName = targetMethodName;
            MethodArguments = methodArguments;
            DependencyAssemblyPaths = dependencyAssemblyPaths;
        }

        /// <summary>
        /// 目标DLL文件的完整路径
        /// </summary>
        public string TargetAssemblyPath { get; set; }

        /// <summary>
        /// 要调用的完整类名（包含命名空间）
        /// </summary>
        public string TargetTypeName { get; set; }

        /// <summary>
        /// 要调用的静态方法名
        /// </summary>
        public string TargetMethodName { get; set; }

        /// <summary>
        /// 传递给目标方法的参数字符串
        /// </summary>
        public string MethodArguments { get; set; }

        /// <summary>
        /// 需要预先加载的依赖程序集路径列表
        /// </summary>
        public List<string> DependencyAssemblyPaths { get; set; }
    }
}
