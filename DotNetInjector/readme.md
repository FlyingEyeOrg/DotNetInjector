# DotNetInjector

## 简介

这是一个 .NET 进程注入工具，该工具的作用是将自定义的 .NET 程序集，注入到目标 .NET 程序中，然后执行自定义的 .NET 代码。
注入方式使用的是远程线程注入，所以注入的 .NET 代码执行的线程环境是一个单独的线程。
该工具支持 .NetCore，.Net Framework，Mono 三种类型的进程注入。

## 注入约定

* 版本约定：framework sdk 的程序需要选择正确的 framework sdk 版本，否则将会注入失败
* 入口类型约定：入口类型不能是嵌套类
* 入口方法约定：入口方法必须有且只有一个 string 类型的参数，返回值为 int，必须是 public static 签名

## 程序原理

使用 c++ 编写了一个 windows 进程注入工具 `injector.exe`，`DotNetInjector` 调用 `injector.exe` 对目标 .net 进程进行注入非托管程序集。
`FrameworkInjectionLibrary` 和 `CoreInjectionLibrary` 以及 `MonoInjectionLibrary` 是三个 C++ 非托管程序集，这三个非托管程序集的作用是将托管的 .NET 程序集注入到 .NET CLR，
然后调用约定的入口方法，在托管环境执行自定义的 .NET 代码。

## 日志

可以在当前 windows 的 `%Temp%` 目录下，查找 `[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-InjectionLogs` 目录，该目录存储的是注入过程的日志文件。

## 软件使用教程

* SDK 版本：如果是 .Net Framework，需要选择/输入对应的 .NET Framework 版本，Core/Mono 选择对应的选项即可
* 托管程序集路径(DLL)：该程序集是需要注入到 .NET 程序 CLR 的托管程序集，类库的 TFM 一定要与目标 .NET 程序 TFM 版本相同
* 调用类型名称：入口类型名称，使用全名，例如 `CoreInjectionClassLibrary.InjectionClass`
* 调用方法名称：入口方法，方法签名必须为 `public static int InjectionMethod(string value)`，否则可能会导致目标进程异常退出，示例：`InjectionMethod`
* 调用方法参数：传递个 `InjectionMethod` 方法的参数，可为空
* 目标进程 Id：需要注入的目标进程 ID，必须是 .NET 进程，否则注入失败

## 类库示例

```c#
using System;
using System.Reflection;

// 一定要使用一个名称空间
namespace CoreInjectionClassLibrary
{
    // 入口类型
    public class InjectionClass
    {
        // 入口方法
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