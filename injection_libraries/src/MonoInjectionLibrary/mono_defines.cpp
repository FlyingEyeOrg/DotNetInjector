#include <Windows.h>
#include <iostream>
#include "mono_defines.h"

// ==================== 辅助函数：加载 mono-2.0-sgen.dll 并获取函数 ====================
bool LoadMonoFunctions()
{
	// 1. 获取已加载的 mono-2.0-sgen.dll 模块句柄
	HMODULE hMono = GetModuleHandleW(L"mono-2.0-sgen.dll");
	if (!hMono)
	{
		std::cerr << "[Error] mono-2.0-sgen.dll is not loaded in this process." << std::endl;
		return false;
	}
	std::cout << "[Info] Found mono-2.0-sgen.dll module." << std::endl;

	// 2. 逐个获取函数地址并验证
	g_mono_get_root_domain = (FnGetRootDomain)GetProcAddress(hMono, "mono_get_root_domain");
	if (!g_mono_get_root_domain)
	{
		std::cerr << "[Error] mono_get_root_domain not found in exports!" << std::endl;
		return false;
	}

	g_mono_domain_assembly_open = (FnMonoDomainAssemblyOpen)GetProcAddress(hMono, "mono_domain_assembly_open");
	if (!g_mono_domain_assembly_open)
	{
		std::cerr << "[Error] mono_domain_assembly_open not found in exports!" << std::endl;
		return false;
	}

	g_mono_assembly_get_image = (FnMonoAssemblyGetImage)GetProcAddress(hMono, "mono_assembly_get_image");
	if (!g_mono_assembly_get_image)
	{
		std::cerr << "[Error] mono_assembly_get_image not found in exports!" << std::endl;
		return false;
	}

	g_mono_class_from_name = (FnMonoClassFromName)GetProcAddress(hMono, "mono_class_from_name");
	if (!g_mono_class_from_name)
	{
		std::cerr << "[Error] mono_class_from_name not found in exports!" << std::endl;
		return false;
	}

	g_mono_method_desc_new = (FnMonoMethodDescNew)GetProcAddress(hMono, "mono_method_desc_new");
	if (!g_mono_method_desc_new)
	{
		std::cerr << "[Error] mono_method_desc_new not found in exports!" << std::endl;
		return false;
	}

	g_mono_method_desc_search_in_class = (FnMonoMethodDescSearchInClass)GetProcAddress(hMono, "mono_method_desc_search_in_class");
	if (!g_mono_method_desc_search_in_class)
	{
		std::cerr << "[Error] mono_method_desc_search_in_class not found in exports!" << std::endl;
		return false;
	}

	g_mono_method_desc_free = (FnMonoMethodDescFree)GetProcAddress(hMono, "mono_method_desc_free");
	if (!g_mono_method_desc_free)
	{
		std::cerr << "[Error] mono_method_desc_free not found in exports!" << std::endl;
		return false;
	}

	g_mono_string_new = (FnMonoStringNew)GetProcAddress(hMono, "mono_string_new");
	if (!g_mono_string_new)
	{
		std::cerr << "[Error] mono_string_new not found in exports!" << std::endl;
		return false;
	}

	g_mono_runtime_invoke = (FnMonoRuntimeInvoke)GetProcAddress(hMono, "mono_runtime_invoke");
	if (!g_mono_runtime_invoke)
	{
		std::cerr << "[Error] mono_runtime_invoke not found in exports!" << std::endl;
		return false;
	}

	g_mono_object_to_string = (FnMonoObjectToString)GetProcAddress(hMono, "mono_object_to_string");
	if (!g_mono_object_to_string)
	{
		std::cerr << "[Error] mono_object_to_string not found in exports!" << std::endl;
		return false;
	}

	g_mono_string_to_utf8 = (FnMonoStringToUtf8)GetProcAddress(hMono, "mono_string_to_utf8");
	if (!g_mono_string_to_utf8)
	{
		std::cerr << "[Error] mono_string_to_utf8 not found in exports!" << std::endl;
		return false;
	}

	g_mono_free = (FnMonoFree)GetProcAddress(hMono, "mono_free");
	if (!g_mono_free)
	{
		std::cerr << "[Error] mono_free not found in exports!" << std::endl;
		return false;
	}

	g_mono_method_signature = (FnMonoMethodSignature)GetProcAddress(hMono, "mono_method_signature");
	if (!g_mono_method_signature)
	{
		std::cerr << "[Error] mono_method_signature not found in exports!" << std::endl;
		return false;
	}

	g_mono_signature_get_return_type = (FnMonoSignatureGetReturnType)GetProcAddress(hMono, "mono_signature_get_return_type");
	if (!g_mono_signature_get_return_type)
	{
		std::cerr << "[Error] mono_signature_get_return_type not found in exports!" << std::endl;
		return false;
	}

	g_mono_type_get_type = (FnMonoTypeGetType)GetProcAddress(hMono, "mono_type_get_type");
	if (!g_mono_type_get_type)
	{
		std::cerr << "[Error] mono_type_get_type not found in exports!" << std::endl;
		return false;
	}

	g_mono_thread_attach = (FnMonoThreadAttach)GetProcAddress(hMono, "mono_thread_attach");
	if (!g_mono_type_get_type)
	{
		std::cerr << "[Error] mono_thread_attach not found in exports!" << std::endl;
		return false;
	}

	g_mono_object_unbox = (FnMonoObjectUnbox)GetProcAddress(hMono, "mono_object_unbox");
	if (!g_mono_type_get_type)
	{
		std::cerr << "[Error] mono_object_unbox not found in exports!" << std::endl;
		return false;
	}


	// 3. 所有必需函数加载成功
	std::cout << "[Info] All required Mono functions loaded successfully via GetProcAddress." << std::endl;
	return true;
}