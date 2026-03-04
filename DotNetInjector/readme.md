# DotNetInjector

## 简介

这是一个 .NET 进程注入工具，该工具的作用是将自定义的 .NET 程序集，注入到目标 .NET 程序中，然后执行自定义的 .NET 代码。
注入方式使用的是远程线程注入，所以注入的 .NET 代码执行的线程环境是一个单独的线程。

## 注入约定

* 版本约定：framework sdk 的程序需要选择正确的 framework sdk 版本，否则将会注入失败
* 入口类型约定：入口类型不能是嵌套类
* 入口方法约定：入口方法必须有且只有一个 string 类型的参数，返回值为 int，必须是 public static 签名

## 类库示例

```c#
using System;
using System.Reflection;

namespace CoreInjectionClassLibrary
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

```