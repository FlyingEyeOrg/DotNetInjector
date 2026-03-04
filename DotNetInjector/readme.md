# DotNetInjector

## 简介

这是一个 .NET 进程注入工具，该工具的作用是将自定义的 .NET 程序集，注入到目标 .NET 程序中，然后执行自定义的 .NET 代码。
注入方式使用的是远程线程注入，所以注入的 .NET 代码执行的线程环境是一个单独的线程。

## 注入约定

* 版本约定：framework sdk 的程序需要选择正确的 framework sdk 版本，否则将会注入失败
* 入口类型约定：入口类型不能是嵌套类
* 入口方法约定：入口方法必须有且只有一个 string 类型的参数，返回值为 int，必须是 public static 签名

## 程序原理

使用 c++ 编写了一个 windows 进程注入工具 `injector.exe`，`DotNetInjector` 调用 `injector.exe` 对目标 .net 进程进行注入非托管程序集。
`FrameworkInjectionLibrary` 和 `CoreInjectionLibrary` 是两个 C++ 非托管程序集，这两个非托管程序集的作用是将托管的 .NET 程序集注入到 .NET CLR，
然后调用约定的入口方法，在托管环境执行自定义的 .NET 代码。

## 日志

可以在目标进程的 exe 目录下，查找 `[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-InjectionLogs` 目录，该目录存储的是注入过程的日志文件。

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