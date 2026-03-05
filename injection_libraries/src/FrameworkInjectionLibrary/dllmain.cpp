// dllmain.cpp
#include "pch.h"
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <system_error>
#include <mscoree.h>   // 包含 ICLRMetaHost 等定义
#include <metahost.h>
#include "logger.h"
#include "injection_parameters.h"
#include "string_converter.h"

#pragma comment(lib, "mscoree.lib")  // 链接 mscoree.lib

// 定义函数指针类型
typedef HRESULT(WINAPI* PFN_CLRCREATEINSTANCE)(
	REFCLSID clsid,
	REFIID   riid,
	void** ppv);

// 全局变量（避免多次初始化）
static bool s_bClrInitialized = false;
static ICLRRuntimeHost* s_pRuntimeHost = nullptr;  // 保存 host 引用，防止 CLR 被关闭

// 辅助函数：打印 HRESULT 错误信息
void PrintHResult(HRESULT hr, const char* context)
{
	Logger::Log("%s failed. HRESULT = 0x%X (%s)",
		context,
		hr,
		std::system_category().message(hr).c_str());
}

// 初始化 CLR 并获取 ICLRRuntimeHost
bool InitializeClrAndGetHost(ICLRRuntimeHost** ppHost)
{
	// 检查是否已经初始化
	if (s_bClrInitialized && s_pRuntimeHost != nullptr)
	{
		Logger::Log("CLR already initialized.");
		*ppHost = s_pRuntimeHost;
		return true;
	}

	Logger::Log("Starting CLR initialization...");

	// 连接共享内存参数
	InjectionParameters parameters;
	auto isOpen = parameters.Open();

	if (!isOpen)
	{
		Logger::Log("Failed to connect to shared memory parameters.");
		return false;
	}
	Logger::Log("Connected to shared memory parameters.");

	// 打印参数摘要
	auto summary = parameters.GetDebugSummary();
	Logger::Log("Injection parameters:\n%s", string_convertor::wstring_to_gbk(summary).c_str());

	// 1. 获取 mscoree.dll 模块句柄
	HMODULE hMscoree = ::GetModuleHandleW(L"mscoree.dll");
	if (!hMscoree)
	{
		Logger::Log("mscoree.dll not found in process.");
		return false;
	}
	Logger::Log("mscoree.dll found in process.");

	// 2. 获取 CLRCreateInstance 函数地址
	PFN_CLRCREATEINSTANCE pfnCLRCreateInstance =
		(PFN_CLRCREATEINSTANCE)::GetProcAddress(hMscoree, "CLRCreateInstance");
	if (!pfnCLRCreateInstance)
	{
		Logger::Log("Failed to get CLRCreateInstance address.");
		return false;
	}
	Logger::Log("Got CLRCreateInstance address.");

	// 3. 调用 CLRCreateInstance 获取 ICLRMetaHost
	ICLRMetaHost* pMetaHost = nullptr;
	HRESULT hr = pfnCLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pMetaHost));
	if (FAILED(hr))
	{
		PrintHResult(hr, "CLRCreateInstance for ICLRMetaHost");
		return false;
	}
	Logger::Log("Created ICLRMetaHost.");

	// 4. 获取指定版本的运行时（.NET 4.0+）
	ICLRRuntimeInfo* pRuntimeInfo = nullptr;
	auto frameworkVersion = parameters.GetFrameworkVersion();
	std::string gbkFrameworkVersion;

	// 如果没有指定版本，使用默认的 .NET Framework 4.0
	if (frameworkVersion.empty())
	{
		frameworkVersion = L"v4.0.30319";
		gbkFrameworkVersion = "v4.0.30319";
		Logger::Log("No framework version specified, using default: v4.0.30319");
	}
	else
	{
		gbkFrameworkVersion = string_convertor::wstring_to_gbk(frameworkVersion);
		Logger::Log("Using framework version from shared memory: %s",
			gbkFrameworkVersion.c_str());
	}

	hr = pMetaHost->GetRuntime(frameworkVersion.c_str(), IID_PPV_ARGS(&pRuntimeInfo));
	if (FAILED(hr))
	{
		std::string context = "GetRuntime for " + gbkFrameworkVersion;
		PrintHResult(hr, context.c_str());
		pMetaHost->Release();
		return false;
	}
	Logger::Log("Got ICLRRuntimeInfo for %s.", gbkFrameworkVersion.c_str());

	// 5. 从 ICLRRuntimeInfo 获取 ICLRRuntimeHost
	hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(ppHost));
	if (FAILED(hr))
	{
		PrintHResult(hr, "GetInterface for ICLRRuntimeHost");
		pRuntimeInfo->Release();
		pMetaHost->Release();
		return false;
	}
	Logger::Log("Got ICLRRuntimeHost.");

	// 6. 使用 host 执行托管代码
	DWORD retval = 0;
	DWORD* pRetVal = &retval;

	auto assemblyFile = parameters.GetAssemblyFile();
	auto entryClass = parameters.GetEntryClass();
	auto entryMethod = parameters.GetEntryMethod();
	auto arg = parameters.GetEntryArgument();

	// 验证必需参数
	if (assemblyFile.empty())
	{
		Logger::Log("Error: Assembly file path is empty.");
		return FALSE;
	}

	if (entryClass.empty())
	{
		Logger::Log("Error: Entry class name is empty.");
		return FALSE;
	}

	if (entryMethod.empty())
	{
		Logger::Log("Error: Entry method name is empty.");
		return FALSE;
	}

	// 将 wstring 转换为 filesystem::path 以便进行文件验证
	std::filesystem::path assemblyPath(assemblyFile);

	// 验证程序集文件是否存在且为有效的 DLL 文件
	if (!std::filesystem::exists(assemblyPath))
	{
		Logger::Log("Error: Assembly file does not exist: %s",
			string_convertor::wstring_to_gbk(assemblyFile).c_str());
		return FALSE;
	}

	if (assemblyPath.extension() != L".dll")
	{
		Logger::Log("Error: Assembly file is not a DLL: %s",
			string_convertor::wstring_to_gbk(assemblyFile).c_str());
		return FALSE;
	}

	Logger::Log("Executing managed method...");
	Logger::Log("  Assembly: %s", string_convertor::wstring_to_gbk(assemblyFile).c_str());
	Logger::Log("  Class:    %s", string_convertor::wstring_to_gbk(entryClass).c_str());
	Logger::Log("  Method:   %s", string_convertor::wstring_to_gbk(entryMethod).c_str());
	Logger::Log("  Argument: %s", string_convertor::wstring_to_gbk(arg).c_str());

	hr = (*ppHost)->ExecuteInDefaultAppDomain(
		assemblyFile.c_str(),
		entryClass.c_str(),
		entryMethod.c_str(),
		arg.c_str(),
		pRetVal);

	if (FAILED(hr))
	{
		PrintHResult(hr, "ExecuteInDefaultAppDomain");
		pRuntimeInfo->Release();
		pMetaHost->Release();
		return false;
	}
	Logger::Log("Managed method executed successfully. Return value: %lu", retval);

	// 7. 保存 host 引用并清理中间接口
	s_pRuntimeHost = *ppHost;
	pRuntimeInfo->Release();
	pMetaHost->Release();

	s_bClrInitialized = true;
	Logger::Log("CLR initialization completed.");
	return true;
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	// 初始化全局日志
	Logger::Initialize();

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		Logger::Log("DLL injected into process (PID: %lu).", GetCurrentProcessId());

		// 禁止线程 attach/detach 通知（提高稳定性）
		DisableThreadLibraryCalls(hModule);

		ICLRRuntimeHost* pRuntimeHost = nullptr;
		if (InitializeClrAndGetHost(&pRuntimeHost))
		{
			Logger::Log("Ready to execute managed code via ICLRRuntimeHost.");
			Logger::Log("ICLRRuntimeHost pointer: 0x%p", pRuntimeHost);

			// 注意：不要立即 Release，否则 CLR 可能被关闭
			// 可根据需要保持引用，或在 DLL_PROCESS_DETACH 时释放
		}
		else
		{
			Logger::Log("CLR initialization failed.");
		}
		break;
	}

	case DLL_THREAD_ATTACH:
		// 无需处理
		break;

	case DLL_THREAD_DETACH:
		// 无需处理
		break;

	case DLL_PROCESS_DETACH:
		// 无需处理
		break;
	}

	return TRUE;
}