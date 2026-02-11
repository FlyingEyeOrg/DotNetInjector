using System.Collections.Generic;
using System.Xml.Serialization;

namespace InjectedClassLibrary
{
    [XmlRoot("InjectionConfig")]
    public class InjectionConfig
    {
        // 无参构造函数用于 XML 反序列化 - 必须初始化所有非空属性
        public InjectionConfig()
        {
            DependencyAssemblyPaths = new List<string>(); // 初始化非空集合
            TargetAssemblyPath = string.Empty;          // 初始化非空字符串
            TargetTypeName = string.Empty;              // 初始化非空字符串
            TargetMethodName = string.Empty;            // 初始化非空字符串
        }

        public InjectionConfig(
            List<string> dependencyAssemblyPaths,
            string targetAssemblyPath,
            string targetTypeName,
            string targetMethodName,
            string? methodArguments = null)
        {
            DependencyAssemblyPaths = dependencyAssemblyPaths;
            TargetAssemblyPath = targetAssemblyPath;
            TargetTypeName = targetTypeName;
            TargetMethodName = targetMethodName;
            MethodArguments = methodArguments;
        }

        /// <summary>
        /// 目标DLL文件的完整路径
        /// </summary>
        [XmlElement("TargetAssemblyPath")]
        public string TargetAssemblyPath { get; set; }

        /// <summary>
        /// 要调用的完整类名（包含命名空间）
        /// </summary>
        [XmlElement("TargetTypeName")]
        public string TargetTypeName { get; set; }

        /// <summary>
        /// 要调用的静态方法名
        /// </summary>
        [XmlElement("TargetMethodName")]
        public string TargetMethodName { get; set; }

        /// <summary>
        /// 传递给目标方法的参数字符串
        /// </summary>
        [XmlElement("MethodArguments")]
        public string? MethodArguments { get; set; }

        /// <summary>
        /// 需要预先加载的依赖程序集路径列表
        /// </summary>
        [XmlArray("DependencyAssemblyPaths")]
        [XmlArrayItem("Path")]
        public List<string> DependencyAssemblyPaths { get; set; }
    }
}