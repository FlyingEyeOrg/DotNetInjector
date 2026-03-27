# DotNetInjector 架构说明

## 目标

`DotNetInjector` 的目标不是做通用 shellcode/manual map 平台，而是围绕“将托管程序集注入到已运行的托管进程并调用指定入口方法”这条链路做稳定交付。

## 目录职责

### `src/DotNetInjector/`

- WPF 主程序。
- `Services/` 负责进程枚举、位数识别、注入器调度、参数桥接。
- `ViewModel/` 负责主窗口状态、命令和用户提示。
- `Tools/x64`、`Tools/x86` 保存注入时真正使用的工具资产。

### `demo/`

- `InjectionDemo/`: `.NET Framework` 目标进程。
- `CoreInjectionDemo/`: `.NET / .NET Core` 目标进程。
- `InjectionClassLibrary/`: Framework / Mono 注入程序集样例。
- `CoreInjectionClassLibrary/`: Core 注入程序集样例。
- `InjectionValidationRunner/`: 端到端联调入口。

### `tests/`

- `DotNetInjector.Tests/`: 单元级行为验证。
- `DotNetInjector.SmokeTests/`: 端到端运行时冒烟 + 主窗口 UI 自动化冒烟。

### `../injection_libraries/src/ManagedInjectionLibrary/`

- 被远程线程加载到目标进程的原生桥接 DLL。
- 负责读取注入参数、识别目标运行时、调用对应托管执行路径。

### `../injection_libraries/src/SharedInjectionLibrary/`

- 运行时检测。
- 请求文件读取与 Base64 解码。
- 原生日志设施。

## 注入链路

1. UI 或验证器创建 `ManagedInjectionRequest`。
2. `ManagedInjectorService` 根据目标进程位数解析 `WinInjector.exe` 和 `ManagedInjectionLibrary.dll`。
3. `InjectionRequestFileBridge` 将请求同时发布到：
   - `%TEMP%\DotNetInjector\requests\request-<pid>.txt`
   - 桥接 DLL 目录下的 `request-<pid>.txt`
   - 桥接 DLL 目录下的 `request.txt`
4. `WinInjector.exe` 将桥接 DLL 注入目标进程。
5. `DllMain` 只负责拉起工作线程，避免在 loader lock 内执行复杂逻辑。
6. 工作线程读取参数，识别运行时，分发到：
   - `.NET Framework`: `ICLRRuntimeHost::ExecuteInDefaultAppDomain`
   - `.NET / .NET Core`: `GetCLRRuntimeHost` + `ExecuteInDefaultAppDomain`
   - `Mono`: 通过导出 API 查找类、方法并调用

## 最近的稳定性改动

- 使用多路径请求文件发布，降低参数桥接失败概率。
- 请求文件读取使用 Win32 API，并增加重试窗口。
- Base64 解码改为更宽容的 `CRYPT_STRING_BASE64_ANY`。
- `Mono` 路径统一改为 UTF-8 字符串传参，并兼容无参入口方法回退。
- 目标位数识别优先使用 `IsWow64Process2`，兼容性更好。
- 新增主窗口 UI 自动化 smoke test，避免 UI 回归只靠人工检查。

## 推荐维护原则

- `ManagedInjectorService` 只做调度，不承载运行时细节。
- 运行时识别与原生执行逻辑尽量留在桥接 DLL 内部。
- Demo 项目只承担验证职责，不要把生产逻辑放进去。
- 新增运行时兼容修复时，必须同时补到 `InjectionValidationRunner` 和 smoke tests。