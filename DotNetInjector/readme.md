# DotNetInjector

一个面向 `.NET Framework`、`.NET / .NET Core`、`Mono` 的托管程序集注入工作台。

当前版本重点解决了三类问题：

- 注入链稳定性：修复了请求文件桥接、跨位数注入器选择、Mono UTF-8 路径和入口解析兼容性。
- UI 可用性：主窗口摘要区、结果区、进程筛选和状态展示更清晰，并补上了可执行的 UI 自动化冒烟。
- 工程化结构：补充共享构建配置、架构文档、独立运行时 smoke tests 和更完整的 README。

## 功能特性

- 支持 `.NET Framework`、`.NET / .NET Core`、`Mono` 目标进程注入。
- 支持 `x86` / `x64` 目标进程，自动选择匹配位数的 `WinInjector.exe` 与 `ManagedInjectionLibrary.dll`。
- 使用请求文件桥接注入参数，规避固定共享内存命名导致的串扰。
- 内置日志、联调 Demo、单元测试、运行时 smoke tests、主窗口 UI smoke test。

## 目录结构

- `src/DotNetInjector/`: WPF 主程序、ViewModel、服务层和工具资产。
- `demo/`: Framework / Core / Mono 联调目标、注入类库和 `InjectionValidationRunner`。
- `tests/`: 单元测试与冒烟测试。
- `docs/`: 架构与项目结构文档。
- `../injection_libraries/src/ManagedInjectionLibrary/`: 原生桥接 DLL。
- `../injection_libraries/src/SharedInjectionLibrary/`: 原生共享运行时解析、参数桥接和日志组件。

详细架构说明见 `docs/architecture.md`。

## 注入约定

- 入口类型不能是嵌套类型。
- 默认入口签名为 `public static int Method(string value)`。
- `Mono` 路径现已兼容同名的无参入口方法回退，但推荐仍使用统一签名，便于三类运行时共享一套程序集。
- `.NET Framework` 注入时，应填写与目标进程匹配的 CLR 版本，例如 `v4.0.30319`。
- 被注入托管程序集的目标框架应与目标运行时兼容。

## 工作原理

1. WPF 主程序解析目标进程位数与运行时类型。
2. `ManagedInjectorService` 选择对应架构的 `WinInjector.exe` 与 `ManagedInjectionLibrary.dll`。
3. 注入请求被发布到临时目录和桥接 DLL 所在目录，供远端进程读取。
4. 原生桥接 DLL 在目标进程中启动工作线程，读取参数并分发到 `.NET Framework`、`.NET / .NET Core`、`Mono` 对应执行路径。
5. 目标托管入口方法执行后，将输出写回目标控制台或日志文件。

## 快速开始

### 1. 构建主程序

```powershell
dotnet build .\src\DotNetInjector\DotNetInjector.csproj -c Debug
```

### 2. 构建原生桥接库

```powershell
& 'D:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' \
  'E:\Projects\DotNetInjector\injection_libraries\src\ManagedInjectionLibrary\ManagedInjectionLibrary.vcxproj' \
  /p:Configuration=Release /p:Platform=x64

& 'D:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' \
  'E:\Projects\DotNetInjector\injection_libraries\src\ManagedInjectionLibrary\ManagedInjectionLibrary.vcxproj' \
  /p:Configuration=Release /p:Platform=Win32
```

然后把输出同步到：

- `src/DotNetInjector/Tools/x64/ManagedInjectionLibrary.dll`
- `src/DotNetInjector/Tools/x86/ManagedInjectionLibrary.dll`

### 3. 运行主程序

```powershell
dotnet run --project .\src\DotNetInjector\DotNetInjector.csproj -c Debug
```

## 联调与测试

### 运行完整验证

```powershell
dotnet run --project .\demo\InjectionValidationRunner\InjectionValidationRunner.csproj -c Release -- all
```

### 只验证单个运行时

```powershell
dotnet run --project .\demo\InjectionValidationRunner\InjectionValidationRunner.csproj -c Release -- framework
dotnet run --project .\demo\InjectionValidationRunner\InjectionValidationRunner.csproj -c Release -- core
dotnet run --project .\demo\InjectionValidationRunner\InjectionValidationRunner.csproj -c Release -- mono
```

### 单元测试

```powershell
dotnet run --project .\tests\DotNetInjector.Tests\DotNetInjector.Tests.csproj -c Release
```

### 冒烟测试

```powershell
dotnet run --project .\tests\DotNetInjector.SmokeTests\DotNetInjector.SmokeTests.csproj -c Release
```

当前 smoke tests 包含：

- `.NET Framework` 注入冒烟
- `.NET / .NET Core` 注入冒烟
- `Mono` 注入冒烟
- 主窗口 UI 自动化冒烟

## 日志与排障

- 注入原生日志目录：`%Temp%\[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-InjectionLogs`
- 如果看到 `.NET Framework` 注入失败，先核对 `Framework 版本` 是否与目标进程一致。
- 如果看到 `Mono` 注入失败，优先确认目标进程确实已加载 `mono-2.0-*.dll`，并检查程序集路径是否包含非 ASCII 字符。
- 如果看到跨位数问题，确认 `Tools/x86/WinInjector.exe` 与 `Tools/x64/WinInjector.exe` 都已存在。

## 示例入口方法

```csharp
using System;
using System.Reflection;

namespace CoreInjectionClassLibrary;

public class InjectionClass
{
    public static int InjectionMethod(string value)
    {
        Console.OutputEncoding = System.Text.Encoding.UTF8;
        Console.WriteLine("print value: " + value);
        Console.WriteLine("方法强制调用成功");
        Console.WriteLine("Entry Assembly: " + Assembly.GetEntryAssembly()?.FullName);
        return 0;
    }
}
```

## 文档

- `docs/architecture.md`: 当前仓库的结构、模块职责和注入链路。
- `BLOG.md`: 本轮修复与工程化整理的博客稿。