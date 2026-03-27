# DotNetInjector：一个 .NET 进程注入工具

本文档按你指定的结构整理：背景、动机、实现原理、支持的目标、使用教程、使用工具与参考文献。适合开发者和研究人员快速上手与二次开发。

## 1 背景

.NET 生态下，调试、运行时修补、插件注入与动态分析等场景经常需要把托管代码注入到正在运行的进程。注入实现跨越托管与本机边界，并受运行时（.NET Framework / .NET Core / Mono）、进程位数与权限策略影响。

传统注入需求包括：自动化测试、运行时诊断、功能性补丁、研究/教学示例。为了把这些场景工程化，需要一套稳健的注入工具与明确的验证流程。

## 2 动机

- 提供一个可重复、可验证的注入链路，覆盖主流 .NET 运行时。 
- 减少调试成本：让失败可复现、可定位（细化到“未能注入 DLL”/“DLL 注入后入口未被调用”）。
- 提供可扩展的注入桥，使托管负载能以一致的方式被加载并执行。

## 3 实现原理（高层）

核心思想：把复杂的跨进程交互拆成三层职责——管理端（发起与展示）、注入桥（本机 DLL，驻留目标进程）、托管负载（被执行的 .NET 程序集）。关键实现要点：

- 参数与控制：使用请求文件（request-file）或共享内存传递注入参数，减小运行时依赖。
- 注入技术：通常通过 `CreateRemoteThread` / `WriteProcessMemory` / `LoadLibrary`（或更高级的 `NtCreateThreadEx`、反射式注入）把注入桥加载到目标进程。
- 运行时识别：注入桥在目标进程内检测运行时类型和进程位数（优先使用 `IsWow64Process2`），然后选择对应加载策略。
- .NET Framework：使用 CLR hosting API 或通过在目标进程中直接 `Assembly.Load` 并调用入口。
- .NET Core/.NET 5+：通过 `nethost`/`hostfxr` 定位并启动 CoreCLR，然后使用 `load_assembly_and_get_function_pointer` 等委托调用托管方法。
- Mono：使用 Mono embedding API（`mono_jit_init`、`mono_domain_assembly_open`、`mono_runtime_invoke`），并在解析入口时支持带字符串参数和无参回退。
- 稳定性策略：避免在 `DllMain` 做繁重工作；把实际加载逻辑放到新线程；对文件竞争、锁定、空文件等情况做短重试。
- 编码一致性：本机<->托管字符串统一使用 UTF-8，避免地区代码页导致的中文乱码问题。

## 4 支持的目标

- 运行时：`.NET Framework`（传统 CLR）、`.NET Core` / `.NET 5+`（CoreCLR/hostfxr）、`Mono`（Embedding）。
- 平台：Windows（x86 / x64）为主；注入桥需与目标进程位数匹配。
- 场景：测试容器、桌面应用、服务进程（需有相应权限）——不适用于未授权或恶意用途。

## 5 使用教程（快速上手）

下面以一个常见流程示例说明如何使用 `DotNetInjector`：

1) 克隆并还原仓库

```powershell
git clone https://github.com/FlyingEyeOrg/DotNetInjector.git
cd DotNetInjector/DotNetInjector
dotnet restore
```

2) 构建托管与测试项目

```powershell
dotnet build src/DotNetInjector/DotNetInjector.csproj -c Debug
dotnet build tests/DotNetInjector.SmokeTests/DotNetInjector.SmokeTests.csproj -c Debug
```

3) 构建原生注入桥（需 MSVC 环境）

```powershell
msbuild injection_libraries/src/ManagedInjectionLibrary/ManagedInjectionLibrary.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild injection_libraries/src/ManagedInjectionLibrary/ManagedInjectionLibrary.vcxproj /p:Configuration=Release /p:Platform=Win32
```

4) 启动目标进程（演示：`demo/CoreInjectionDemo`）并记录 PID

5) 使用注入工具（参考下一节）把注入桥 DLL 注入到目标进程

6) 启动 `InjectionValidationRunner` 或使用 `DotNetInjector` 管理端触发注入并观察输出

示例：假设使用外部注入器 `wininjector`，且注入桥位于 `src/DotNetInjector/Tools/x64/ManagedInjectionLibrary.dll`：

```powershell
# 在管理员 PowerShell 中运行（视权限需求而定）
git clone https://github.com/FlyingEyeOrg/wininjector.git
cd wininjector
msbuild wininjector.sln /p:Configuration=Release
.
\path\to\wininjector\bin\Release\wininjector.exe -pid 1234 -dll "..\DotNetInjector\src\DotNetInjector\Tools\x64\ManagedInjectionLibrary.dll"
```

注：`wininjector` 的命令行与参数请参考其仓库的 README；该示例仅示意注入流程。

## 6 使用工具：`wininjector`

仓库： https://github.com/FlyingEyeOrg/wininjector.git

简介：`wininjector` 是一个用于在 Windows 上注入 DLL 的工具（示例/参考）。推荐流程：

- 从上游仓库拉取并构建 `wininjector`（Visual Studio / msbuild）。
- 使用 `wininjector` 将注入桥（`ManagedInjectionLibrary.dll`）注入到目标进程的地址空间。
- 注入桥会读取请求文件并根据参数触发托管负载的加载与执行。

示意命令已在第五节给出；如果需要，我可以为你生成基于 `wininjector` 的更完整示例脚本，包括自动检测目标 PID、按位数选择 x86/x64 DLL 并执行注入。

## 7 参考文献与延伸阅读

- .NET Hosting APIs / hostfxr（CoreCLR hosting）— 官方文档
- nethost 与 hostfxr 示例 — dotnet/runtime 仓库
- Mono Embedding API — Mono 官方文档
- Windows API：`CreateRemoteThread`、`WriteProcessMemory`、`IsWow64Process2` — Microsoft Docs
- 注入相关原理与防护：各类安全研究与 MS docs