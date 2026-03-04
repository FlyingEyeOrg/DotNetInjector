// dllmain.cpp : 定义 DLL 应用程序的入口点。
// 说明：这是 DLL 的主源文件，包含 DLL 入口函数 DllMain。
// 预编译头文件，用于加快编译速度，通常包含常用头文件和项目设置。
#include "pch.h"
#include "logger.h"
#include "injection_parameters.h"

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/threads.h>

// ==================== 定义函数指针类型（仅使用导出的函数） ====================
// mono_domain_get
typedef MonoDomain* (__cdecl* FnGetRootDomain)();

// mono_domain_assembly_open
typedef MonoAssembly* (__cdecl* FnMonoDomainAssemblyOpen)(MonoDomain* domain, const char* name);

// mono_assembly_get_image
typedef MonoImage* (__cdecl* FnMonoAssemblyGetImage)(MonoAssembly* assembly);

// mono_class_from_name
typedef MonoClass* (__cdecl* FnMonoClassFromName)(MonoImage* image, const char* namesp, const char* name);

// mono_method_desc_new
typedef MonoMethodDesc* (__cdecl* FnMonoMethodDescNew)(const char* name, mono_bool include_namespace);

// mono_method_desc_search_in_class
typedef MonoMethod* (__cdecl* FnMonoMethodDescSearchInClass)(MonoMethodDesc* desc, MonoClass* klass);

// mono_method_desc_free
typedef void(__cdecl* FnMonoMethodDescFree)(MonoMethodDesc* desc);

// mono_string_new
typedef MonoString* (__cdecl* FnMonoStringNew)(MonoDomain* domain, const char* text);

// mono_runtime_invoke
typedef MonoObject* (__cdecl* FnMonoRuntimeInvoke)(MonoMethod* method, void* obj, void** params, MonoObject** exc);

// mono_object_to_string
typedef MonoString* (__cdecl* FnMonoObjectToString)(MonoObject* obj, MonoObject** exc);

// mono_string_to_utf8
typedef char* (__cdecl* FnMonoStringToUtf8)(MonoString* s);

// mono_free
typedef void(__cdecl* FnMonoFree)(void* ptr);

// mono_method_signature
typedef MonoMethodSignature* (__cdecl* FnMonoMethodSignature)(MonoMethod* method);

// mono_signature_get_return_type
typedef MonoType* (__cdecl* FnMonoSignatureGetReturnType)(MonoMethodSignature* sig);

// mono_type_get_type
typedef MonoTypeEnum(__cdecl* FnMonoTypeGetType)(MonoType* type);

// mono_thread_attach
typedef MonoThread* (__cdecl* FnMonoThreadAttach)(MonoDomain* domain);

// mono_object_unbox
typedef void* (__cdecl* FnMonoObjectUnbox)(MonoObject* obj);

// ==================== 全局函数指针 ====================
static FnGetRootDomain g_mono_get_root_domain = nullptr;
static FnMonoDomainAssemblyOpen g_mono_domain_assembly_open = nullptr;
static FnMonoAssemblyGetImage g_mono_assembly_get_image = nullptr;
static FnMonoClassFromName g_mono_class_from_name = nullptr;
static FnMonoMethodDescNew g_mono_method_desc_new = nullptr;
static FnMonoMethodDescSearchInClass g_mono_method_desc_search_in_class = nullptr;
static FnMonoMethodDescFree g_mono_method_desc_free = nullptr;
static FnMonoStringNew g_mono_string_new = nullptr;
static FnMonoRuntimeInvoke g_mono_runtime_invoke = nullptr;
static FnMonoObjectToString g_mono_object_to_string = nullptr;
static FnMonoStringToUtf8 g_mono_string_to_utf8 = nullptr;
static FnMonoFree g_mono_free = nullptr;
static FnMonoMethodSignature g_mono_method_signature = nullptr;
static FnMonoSignatureGetReturnType g_mono_signature_get_return_type = nullptr;
static FnMonoTypeGetType g_mono_type_get_type = nullptr;
static FnMonoThreadAttach g_mono_thread_attach = nullptr;
static FnMonoObjectUnbox g_mono_object_unbox = nullptr;

// 加载所有 mono 函数
bool LoadMonoFunctions();

// ==================== DLL 入口函数 ====================
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	Logger::Initialize();

	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		if (!LoadMonoFunctions())
			return FALSE;

		// 1. 获取当前 Mono Domain
		MonoDomain* domain = g_mono_get_root_domain();
		if (!domain)
		{
			Logger::Log("mono_get_root_domain() returned nullptr. Is Mono initialized?");
			return FALSE;
		}
		Logger::Log("Got MonoDomain successfully.");

		// 2. 当前线程附加到 mono 运行时
		MonoThread* attachedThread = g_mono_thread_attach(domain);
		if (!attachedThread)
		{
			Logger::Log("mono_thread_attach() failed!");
			return FALSE;
		}
		Logger::Log("Thread attached to Mono runtime.");

		// 3. 加载程序集
		const char* assemblyPath = "E:\\Projects\\DotNetInjector\\DotNetInjector\\demo\\InjectionClassLibrary\\bin\\Debug\\InjectionClassLibrary.dll";
		MonoAssembly* assembly = g_mono_domain_assembly_open(domain, assemblyPath);
		if (!assembly)
		{
			Logger::Log("Failed to open assembly: ");
			Logger::Log(assemblyPath);
			return FALSE;
		}
		Logger::Log("Loaded assembly: ");
		Logger::Log(assemblyPath);

		// 4. 获取 Image
		MonoImage* image = g_mono_assembly_get_image(assembly);
		if (!image)
		{
			Logger::Log("Failed to get image from assembly.");
			return FALSE;
		}
		Logger::Log("Got MonoImage successfully.");

		// 5. 查找类
		const char* namespaceName = "InjectionClassLibrary";
		const char* className = "InjectionClass";
		MonoClass* klass = g_mono_class_from_name(image, namespaceName, className);
		if (!klass)
		{
			Logger::Log("Class not found: ");
			Logger::Log(namespaceName);
			Logger::Log(".");
			Logger::Log(className);
			return FALSE;
		}
		Logger::Log("Found class: ");
		Logger::Log(namespaceName);
		Logger::Log(".");
		Logger::Log(className);

		// 6. 创建方法描述符
		const char* methodDescStr = "InjectionClassLibrary.InjectionClass::InjectionMethod(string)";
		MonoMethodDesc* desc = g_mono_method_desc_new(methodDescStr, TRUE);
		if (!desc)
		{
			Logger::Log("Failed to create method descriptor: ");
			Logger::Log(methodDescStr);
			return FALSE;
		}
		Logger::Log("Created method descriptor: ");
		Logger::Log(methodDescStr);

		// 7. 查找方法
		MonoMethod* method = g_mono_method_desc_search_in_class(desc, klass);
		g_mono_method_desc_free(desc);
		if (!method)
		{
			Logger::Log("Method not found in class.");
			return FALSE;
		}
		Logger::Log("Found method: InjectionMethod");

		// 8. 创建参数
		const char* argStr = "World";
		MonoString* arg = g_mono_string_new(domain, argStr);
		if (!arg)
		{
			Logger::Log("Failed to create MonoString for argument.");
			return FALSE;
		}
		Logger::Log("Created argument string: ");
		Logger::Log(argStr);

		void* args[1] = { arg };
		MonoObject* exc = nullptr;

		// 9. 调用方法
		MonoObject* result = g_mono_runtime_invoke(method, nullptr, args, &exc);
		if (!result && !exc)
		{
			Logger::Log("Method returned null and no exception.");
		}
		else if (exc)
		{
			MonoString* exMsg = g_mono_object_to_string(exc, nullptr);
			if (exMsg)
			{
				char* utf8Ex = g_mono_string_to_utf8(exMsg);
				Logger::Log("Exception in C#: ");
				Logger::Log(utf8Ex);
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
						Logger::Log("Result (int): ");
						Logger::Log(std::to_string(intValue).c_str());
					}
					else
					{
						Logger::Log("Expected int but got null.");
					}
				}
				else
				{
					Logger::Log("Unexpected return type: ");
					Logger::Log(std::to_string(retTypeEnum).c_str());
					Logger::Log(", expected MONO_TYPE_I4 (int).");
					return FALSE;
				}
			}
			else
			{
				Logger::Log("Could not get method signature.");
			}
		}
	}

	return TRUE;
}

// ==================== 辅助函数：加载 mono-2.0-sgen.dll 并获取函数 ====================
bool LoadMonoFunctions()
{
	// 1. 获取已加载的 mono-2.0-sgen.dll 模块句柄
	HMODULE hMono = GetModuleHandleW(L"mono-2.0-sgen.dll");
	if (!hMono)
	{
		Logger::Log("mono-2.0-sgen.dll is not loaded in this process.");
		return false;
	}
	Logger::Log("Found mono-2.0-sgen.dll module.");

	// 2. 逐个获取函数地址并验证
	g_mono_get_root_domain = (FnGetRootDomain)GetProcAddress(hMono, "mono_get_root_domain");
	if (!g_mono_get_root_domain)
	{
		Logger::Log("mono_get_root_domain not found in exports!");
		return false;
	}

	g_mono_domain_assembly_open = (FnMonoDomainAssemblyOpen)GetProcAddress(hMono, "mono_domain_assembly_open");
	if (!g_mono_domain_assembly_open)
	{
		Logger::Log("mono_domain_assembly_open not found in exports!");
		return false;
	}

	g_mono_assembly_get_image = (FnMonoAssemblyGetImage)GetProcAddress(hMono, "mono_assembly_get_image");
	if (!g_mono_assembly_get_image)
	{
		Logger::Log("mono_assembly_get_image not found in exports!");
		return false;
	}

	g_mono_class_from_name = (FnMonoClassFromName)GetProcAddress(hMono, "mono_class_from_name");
	if (!g_mono_class_from_name)
	{
		Logger::Log("mono_class_from_name not found in exports!");
		return false;
	}

	g_mono_method_desc_new = (FnMonoMethodDescNew)GetProcAddress(hMono, "mono_method_desc_new");
	if (!g_mono_method_desc_new)
	{
		Logger::Log("mono_method_desc_new not found in exports!");
		return false;
	}

	g_mono_method_desc_search_in_class = (FnMonoMethodDescSearchInClass)GetProcAddress(hMono, "mono_method_desc_search_in_class");
	if (!g_mono_method_desc_search_in_class)
	{
		Logger::Log("mono_method_desc_search_in_class not found in exports!");
		return false;
	}

	g_mono_method_desc_free = (FnMonoMethodDescFree)GetProcAddress(hMono, "mono_method_desc_free");
	if (!g_mono_method_desc_free)
	{
		Logger::Log("mono_method_desc_free not found in exports!");
		return false;
	}

	g_mono_string_new = (FnMonoStringNew)GetProcAddress(hMono, "mono_string_new");
	if (!g_mono_string_new)
	{
		Logger::Log("mono_string_new not found in exports!");
		return false;
	}

	g_mono_runtime_invoke = (FnMonoRuntimeInvoke)GetProcAddress(hMono, "mono_runtime_invoke");
	if (!g_mono_runtime_invoke)
	{
		Logger::Log("mono_runtime_invoke not found in exports!");
		return false;
	}

	g_mono_object_to_string = (FnMonoObjectToString)GetProcAddress(hMono, "mono_object_to_string");
	if (!g_mono_object_to_string)
	{
		Logger::Log("mono_object_to_string not found in exports!");
		return false;
	}

	g_mono_string_to_utf8 = (FnMonoStringToUtf8)GetProcAddress(hMono, "mono_string_to_utf8");
	if (!g_mono_string_to_utf8)
	{
		Logger::Log("mono_string_to_utf8 not found in exports!");
		return false;
	}

	g_mono_free = (FnMonoFree)GetProcAddress(hMono, "mono_free");
	if (!g_mono_free)
	{
		Logger::Log("mono_free not found in exports!");
		return false;
	}

	g_mono_method_signature = (FnMonoMethodSignature)GetProcAddress(hMono, "mono_method_signature");
	if (!g_mono_method_signature)
	{
		Logger::Log("mono_method_signature not found in exports!");
		return false;
	}

	g_mono_signature_get_return_type = (FnMonoSignatureGetReturnType)GetProcAddress(hMono, "mono_signature_get_return_type");
	if (!g_mono_signature_get_return_type)
	{
		Logger::Log("mono_signature_get_return_type not found in exports!");
		return false;
	}

	g_mono_type_get_type = (FnMonoTypeGetType)GetProcAddress(hMono, "mono_type_get_type");
	if (!g_mono_type_get_type)
	{
		Logger::Log("mono_type_get_type not found in exports!");
		return false;
	}

	g_mono_thread_attach = (FnMonoThreadAttach)GetProcAddress(hMono, "mono_thread_attach");
	if (!g_mono_thread_attach)
	{
		Logger::Log("mono_thread_attach not found in exports!");
		return false;
	}

	g_mono_object_unbox = (FnMonoObjectUnbox)GetProcAddress(hMono, "mono_object_unbox");
	if (!g_mono_object_unbox)
	{
		Logger::Log("mono_object_unbox not found in exports!");
		return false;
	}

	// 3. 所有必需函数加载成功
	Logger::Log("All required Mono functions loaded successfully via GetProcAddress.");
	return true;
}