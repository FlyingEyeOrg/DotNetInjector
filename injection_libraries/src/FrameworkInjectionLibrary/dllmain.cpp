// dllmain.cpp
#include "pch.h"
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <system_error>
#include <mscoree.h>   // 包含 ICLRMetaHost 等定义
#include <metahost.h>
#include "logger.h"

#pragma comment(lib, "mscoree.lib")  // 链接 mscoree.lib

// 定义函数指针类型
typedef HRESULT(WINAPI* PFN_CLRCREATEINSTANCE)(
	REFCLSID clsid,
	REFIID   riid,
	void** ppv);

// 全局变量（避免多次初始化）
static bool s_bClrInitialized = false;

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
	if (s_bClrInitialized)
	{
		Logger::Log("CLR already initialized.");
		return true;
	}

	// 1. 获取 mscoree.dll 模块句柄
	HMODULE hMscoree = ::GetModuleHandleW(L"mscoree.dll");
	if (!hMscoree)
	{
		Logger::Log("mscoree.dll not found in process.");
		return false;
	}
	Logger::Log("mscoree.dll found.");

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
	Logger::Log("Got ICLRMetaHost.");

	// 4. 获取指定版本的运行时（.NET 4.0+）
	ICLRRuntimeInfo* pRuntimeInfo = nullptr;
	hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_PPV_ARGS(&pRuntimeInfo));
	if (FAILED(hr))
	{
		PrintHResult(hr, "GetRuntime");
		pMetaHost->Release();
		return false;
	}
	Logger::Log("Got ICLRRuntimeInfo for v4.0.30319.");

	// 5. 从 ICLRRuntimeInfo 获取 ICLRRuntimeHost
	hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(ppHost));
	if (FAILED(hr))
	{
		PrintHResult(hr, "GetInterface for ICLRRuntimeHost");
		pRuntimeInfo->Release();
		pMetaHost->Release();
		return false;
	}

	// 6. 使用host
	ICLRRuntimeHost* host = (*ppHost);
	DWORD* retval = nullptr;

	hr = host->ExecuteInDefaultAppDomain(
		L"E:\\users\\lanxf01\\Desktop\\demo\\FrameworkWpfAppDemo\\MyClassLibrary\\bin\\Debug\\net472\\MyClassLibrary.dll",
		L"MyClassLibrary.MainWindow",
		L"Print",
		L"123131",
		retval);

	if (FAILED(hr))
	{
		PrintHResult(hr, "Call MyClassLibrary.MainWindow.Print");
		pRuntimeInfo->Release();
		pMetaHost->Release();
		return false;
	}

	// 7. 清理中间接口（保留 ppHost）
	pRuntimeInfo->Release();
	pMetaHost->Release();

	s_bClrInitialized = true;
	return true;
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		// 禁止线程 attach/detach 通知（提高稳定性）
		DisableThreadLibraryCalls(hModule);

		Logger::Log("DLL injected into process...");

		ICLRRuntimeHost* pRuntimeHost = nullptr;
		if (InitializeClrAndGetHost(&pRuntimeHost))
		{
			// TODO: 使用 pRuntimeHost 执行托管代码
			// 例如：pRuntimeHost->ExecuteInDefaultAppDomain(...);
			Logger::Log("Ready to execute managed code via ICLRRuntimeHost.");

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
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}