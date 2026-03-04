// dllmain.cpp : 定义 DLL 应用程序的入口点。
// 说明：这是 DLL 的主源文件，包含 DLL 入口函数 DllMain。

#include "pch.h"
#include "mono_defines.h"
// 预编译头文件，用于加快编译速度，通常包含常用头文件和项目设置。

#include <iostream>
#include <windows.h>

// ==================== DLL 入口函数 ====================
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		if (!LoadMonoFunctions())
			return FALSE;

		// 1. 获取当前 Mono Domain
		MonoDomain* domain = g_mono_get_root_domain();
		if (!domain)
		{
			std::cerr << "[Error] mono_get_root_domain() returned nullptr. Is Mono initialized?" << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Got MonoDomain successfully." << std::endl;

		// 2. 当前线程附加到 mono 运行时
		MonoThread* attachedThread = g_mono_thread_attach(domain);
		if (!attachedThread)
		{
			std::cerr << "[Error] mono_thread_attach() failed!" << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Thread attached to Mono runtime." << std::endl;

		// 3. 加载程序集
		const char* assemblyPath = "E:\\Projects\\DotNetInjector\\DotNetInjector\\demo\\InjectionClassLibrary\\bin\\Debug\\InjectionClassLibrary.dll";
		MonoAssembly* assembly = g_mono_domain_assembly_open(domain, assemblyPath);
		if (!assembly)
		{
			std::cerr << "[Error] Failed to open assembly: " << assemblyPath << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Loaded assembly: " << assemblyPath << std::endl;

		// 4. 获取 Image
		MonoImage* image = g_mono_assembly_get_image(assembly);
		if (!image)
		{
			std::cerr << "[Error] Failed to get image from assembly." << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Got MonoImage successfully." << std::endl;

		// 5. 查找类
		const char* namespaceName = "InjectionClassLibrary";
		const char* className = "InjectionClass";
		MonoClass* klass = g_mono_class_from_name(image, namespaceName, className);
		if (!klass)
		{
			std::cerr << "[Error] Class not found: " << namespaceName << "." << className << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Found class: " << namespaceName << "." << className << std::endl;

		// 6. 创建方法描述符
		const char* methodDescStr = "InjectionClassLibrary.InjectionClass::InjectionMethod(string)";
		MonoMethodDesc* desc = g_mono_method_desc_new(methodDescStr, TRUE);
		if (!desc)
		{
			std::cerr << "[Error] Failed to create method descriptor: " << methodDescStr << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Created method descriptor: " << methodDescStr << std::endl;

		// 7. 查找方法
		MonoMethod* method = g_mono_method_desc_search_in_class(desc, klass);
		g_mono_method_desc_free(desc);
		if (!method)
		{
			std::cerr << "[Error] Method not found in class." << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Found method: InjectionMethod" << std::endl;

		// 8. 创建参数
		const char* argStr = "World";
		MonoString* arg = g_mono_string_new(domain, argStr);
		if (!arg)
		{
			std::cerr << "[Error] Failed to create MonoString for argument." << std::endl;
			return FALSE;
		}
		std::cout << "[Info] Created argument string: " << argStr << std::endl;

		void* args[1] = { arg };
		MonoObject* exc = nullptr;

		// 9. 调用方法
		MonoObject* result = g_mono_runtime_invoke(method, nullptr, args, &exc);
		if (!result && !exc)
		{
			std::cerr << "[Warning] Method returned null and no exception." << std::endl;
		}
		else if (exc)
		{
			MonoString* exMsg = g_mono_object_to_string(exc, nullptr);
			if (exMsg)
			{
				char* utf8Ex = g_mono_string_to_utf8(exMsg);
				std::cerr << "[Error] Exception in C#: " << utf8Ex << std::endl;
				g_mono_free(utf8Ex);
			}
			return FALSE;
		}
		else
		{
			// 处理结果
			MonoMethodSignature* sig = g_mono_method_signature(method);
			if (sig)
			{
				MonoType* retType = g_mono_signature_get_return_type(sig);
				MonoTypeEnum retTypeEnum = g_mono_type_get_type(retType);

				// 约定只返回 int
				if (retTypeEnum == MONO_TYPE_I4)
				{
					if (result)
					{
						int intValue = *(int*)g_mono_object_unbox(result);
						std::cout << "[Success] Result (int): " << intValue << std::endl;
					}
					else
					{
						std::cout << "[Warning] Expected int but got null." << std::endl;
					}
				}
				else
				{
					std::cerr << "[Error] Unexpected return type: " << retTypeEnum << ", expected MONO_TYPE_I4 (int)." << std::endl;
					return FALSE;
				}
			}
			else
			{
				std::cerr << "[Warning] Could not get method signature." << std::endl;
			}
		}
	}

	return TRUE;
}