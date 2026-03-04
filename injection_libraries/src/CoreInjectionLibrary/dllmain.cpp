// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <iostream>
// 引用 .net clr 垫片头文件
#include <mscoree.h>
#include "string_converter.h"
#include "injection_parameters.h"
#include "logger.h"

ICLRRuntimeHost* GetCLRRuntimeHost();
typedef HRESULT(STDAPICALLTYPE* FnGetCLRRuntimeHost)(REFIID riid, IUnknown** pUnk);

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved)
{
	Logger::Initialize();

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		// 连接共享内存参数
		InjectionParameters parameters;
		if (!parameters.Open())
		{
			Logger::Log("Failed to connect to shared memory parameters.");
			return FALSE;
		}
		Logger::Log("Connected to shared memory parameters.");

		auto summary = parameters.GetDebugSummary();
		Logger::Log("Injection parameters:\n%s", string_convertor::wstring_to_gbk(summary).c_str());

		auto host = GetCLRRuntimeHost();
		if (!host)
		{
			Logger::Log("Failed to get ICLRRuntimeHost.");
			return FALSE;
		}

		auto assemblyFile = parameters.GetAssemblyFile();
		auto entryClass = parameters.GetEntryClass();
		auto entryMethod = parameters.GetEntryMethod();
		auto arg = parameters.GetEntryArgument();

		Logger::Log("Executing managed method...");
		Logger::Log("  Assembly: %s", string_convertor::wstring_to_gbk(assemblyFile).c_str());
		Logger::Log("  Class:    %s", string_convertor::wstring_to_gbk(entryClass).c_str());
		Logger::Log("  Method:   %s", string_convertor::wstring_to_gbk(entryMethod).c_str());
		Logger::Log("  Argument: %s", string_convertor::wstring_to_gbk(arg).c_str());

		DWORD retval = 0;
		HRESULT hr = host->ExecuteInDefaultAppDomain(
			assemblyFile.c_str(),
			entryClass.c_str(),
			entryMethod.c_str(),
			arg.c_str(),
			&retval);

		if (FAILED(hr))
		{
			Logger::Log("ExecuteInDefaultAppDomain failed with HRESULT=0x%08X", hr);
			return FALSE;
		}

		Logger::Log("Managed method executed successfully. Return value=%lu", retval);
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

typedef HRESULT(STDAPICALLTYPE* FnGetCLRRuntimeHost)(REFIID riid, IUnknown** pUnk);

// Returns the ICLRRuntimeHost instance or nullptr on failure.
ICLRRuntimeHost* GetCLRRuntimeHost()
{
	HMODULE coreCLRModule = ::GetModuleHandle(L"coreclr.dll");
	if (!coreCLRModule)
	{
		coreCLRModule = ::GetModuleHandle(L"coreclr.so");
	}
	if (!coreCLRModule)
	{
		coreCLRModule = ::GetModuleHandle(L"coreclr.dynlib");
	}
	if (!coreCLRModule)
	{
		return nullptr;
	}

	FnGetCLRRuntimeHost pfnGetCLRRuntimeHost = (FnGetCLRRuntimeHost)::GetProcAddress(coreCLRModule, "GetCLRRuntimeHost");
	if (!pfnGetCLRRuntimeHost)
	{
		return nullptr;
	}

	ICLRRuntimeHost* clrRuntimeHost = nullptr;
	HRESULT hr = pfnGetCLRRuntimeHost(IID_ICLRRuntimeHost, (IUnknown**)&clrRuntimeHost);
	if (FAILED(hr)) {
		return nullptr;
	}

	return clrRuntimeHost;
}

