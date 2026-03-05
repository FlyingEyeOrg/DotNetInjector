// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "logger.h"
#include "injection_parameters.h"
#include "string_converter.h"

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/threads.h>
#include <string>
#include <vector>

// ==================== 定义函数指针类型 ====================
typedef MonoDomain* (__cdecl* FnGetRootDomain)();
typedef MonoAssembly* (__cdecl* FnMonoDomainAssemblyOpen)(MonoDomain* domain, const char* name);
typedef MonoImage* (__cdecl* FnMonoAssemblyGetImage)(MonoAssembly* assembly);
typedef MonoClass* (__cdecl* FnMonoClassFromName)(MonoImage* image, const char* namesp, const char* name);
typedef MonoMethodDesc* (__cdecl* FnMonoMethodDescNew)(const char* name, mono_bool include_namespace);
typedef MonoMethod* (__cdecl* FnMonoMethodDescSearchInClass)(MonoMethodDesc* desc, MonoClass* klass);
typedef void(__cdecl* FnMonoMethodDescFree)(MonoMethodDesc* desc);
typedef MonoString* (__cdecl* FnMonoStringNew)(MonoDomain* domain, const char* text);
typedef MonoObject* (__cdecl* FnMonoRuntimeInvoke)(MonoMethod* method, void* obj, void** params, MonoObject** exc);
typedef MonoString* (__cdecl* FnMonoObjectToString)(MonoObject* obj, MonoObject** exc);
typedef char* (__cdecl* FnMonoStringToUtf8)(MonoString* s);
typedef void(__cdecl* FnMonoFree)(void* ptr);
typedef MonoMethodSignature* (__cdecl* FnMonoMethodSignature)(MonoMethod* method);
typedef MonoType* (__cdecl* FnMonoSignatureGetReturnType)(MonoMethodSignature* sig);
typedef MonoTypeEnum(__cdecl* FnMonoTypeGetType)(MonoType* type);
typedef MonoThread* (__cdecl* FnMonoThreadAttach)(MonoDomain* domain);
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

// ==================== 函数声明 ====================
bool LoadMonoFunctions();
void ExtractNamespaceAndClass(const std::string& fullName, std::string& outNamespace, std::string& outClass);
bool ValidateParameter(const char* param, const char* paramName);

// ==================== DLL 入口函数 ====================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	Logger::Initialize();

	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		// 1. 加载 Mono 函数
		if (!LoadMonoFunctions())
		{
			Logger::Log("Failed to load Mono functions.");
			return FALSE;
		}

		// 2. 连接共享内存参数
		InjectionParameters parameters;
		if (!parameters.Open())
		{
			Logger::Log("Failed to connect to shared memory parameters.");
			return FALSE;
		}
		Logger::Log("Connected to shared memory parameters.");

		// 3. 获取并验证共享内存参数
		auto assemblyFile = parameters.GetAssemblyFile();
		auto entryClass = parameters.GetEntryClass();
		auto entryMethod = parameters.GetEntryMethod();
		auto arg = parameters.GetEntryArgument();

		// 4. 转换参数编码
		auto gbkAssemblyFile = string_convertor::wstring_to_gbk(assemblyFile);
		auto gbkEntryClass = string_convertor::wstring_to_gbk(entryClass);
		auto gbkEntryMethod = string_convertor::wstring_to_gbk(entryMethod);
		auto gbkArg = string_convertor::wstring_to_gbk(arg);

		// 验证转换后非空
		if (!ValidateParameter(gbkAssemblyFile.c_str(), "AssemblyFile(GBK)"))
			return FALSE;
		if (!ValidateParameter(gbkEntryClass.c_str(), "EntryClass(GBK)"))
			return FALSE;
		if (!ValidateParameter(gbkEntryMethod.c_str(), "EntryMethod(GBK)"))
			return FALSE;

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

		// 5. 记录参数日志
		Logger::Log("Executing managed method...");
		Logger::Log("  Assembly: %s", gbkAssemblyFile.c_str());
		Logger::Log("  Class:    %s", gbkEntryClass.c_str());
		Logger::Log("  Method:   %s", gbkEntryMethod.c_str());
		Logger::Log("  Argument: %s", gbkArg.c_str());

		// 6. 获取 Mono Domain
		MonoDomain* domain = g_mono_get_root_domain();
		if (!domain)
		{
			Logger::Log("mono_get_root_domain() returned nullptr. Is Mono initialized?");
			return FALSE;
		}
		Logger::Log("Got MonoDomain successfully.");

		// 7. 线程附加到 Mono 运行时
		MonoThread* attachedThread = g_mono_thread_attach(domain);
		if (!attachedThread)
		{
			Logger::Log("mono_thread_attach() failed!");
			return FALSE;
		}
		Logger::Log("Thread attached to Mono runtime.");

		// 8. 加载程序集
		const char* assemblyPath = gbkAssemblyFile.c_str();
		MonoAssembly* assembly = g_mono_domain_assembly_open(domain, assemblyPath);
		if (!assembly)
		{
			Logger::Log("Failed to open assembly: %s", assemblyPath);
			return FALSE;
		}
		Logger::Log("Loaded assembly: %s", assemblyPath);

		// 9. 获取 Image
		MonoImage* image = g_mono_assembly_get_image(assembly);
		if (!image)
		{
			Logger::Log("Failed to get image from assembly.");
			return FALSE;
		}
		Logger::Log("Got MonoImage successfully.");

		// 10. 提取命名空间和类名
		std::string className, namespaceName;
		ExtractNamespaceAndClass(gbkEntryClass, namespaceName, className);

		// 11. 查找类
		MonoClass* klass = g_mono_class_from_name(image, namespaceName.c_str(), className.c_str());
		if (!klass)
		{
			Logger::Log("Class not found: %s.%s", namespaceName.c_str(), className.c_str());
			return FALSE;
		}
		Logger::Log("Found class: %s.%s", namespaceName.c_str(), className.c_str());

		// 12. 创建方法描述符
		std::string methodDescStr = gbkEntryClass + "::" + gbkEntryMethod + "(string)";
		MonoMethodDesc* desc = g_mono_method_desc_new(methodDescStr.c_str(), namespaceName.empty() ? FALSE : TRUE);
		if (!desc)
		{
			Logger::Log("Failed to create method descriptor: %s", methodDescStr.c_str());
			return FALSE;
		}
		Logger::Log("Created method descriptor: %s", methodDescStr.c_str());

		// 13. 查找方法
		MonoMethod* method = g_mono_method_desc_search_in_class(desc, klass);
		g_mono_method_desc_free(desc);
		if (!method)
		{
			Logger::Log("Method not found in class: %s", gbkEntryMethod.c_str());
			return FALSE;
		}
		Logger::Log("Found method: %s", gbkEntryMethod.c_str());

		// 14. 创建参数
		MonoString* monoArg = g_mono_string_new(domain, gbkArg.c_str());
		if (!monoArg)
		{
			Logger::Log("Failed to create MonoString for argument.");
			return FALSE;
		}
		Logger::Log("Created argument string: %s", gbkArg.c_str());

		void* args[1] = { monoArg };
		MonoObject* exc = nullptr;

		// 15. 调用方法
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
				Logger::Log("Exception in C#: %s", utf8Ex);
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

				if (retTypeEnum == MONO_TYPE_I4)
				{
					if (result)
					{
						int intValue = *(int*)g_mono_object_unbox(result);
						Logger::Log("Result (int): %d", intValue);
					}
					else
					{
						Logger::Log("Expected int but got null.");
					}
				}
				else
				{
					Logger::Log("Unexpected return type: %d, expected MONO_TYPE_I4 (int).", retTypeEnum);
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

// ==================== 辅助函数 ====================

// 验证 C 字符串参数
bool ValidateParameter(const char* param, const char* paramName)
{
	if (!param || strlen(param) == 0)
	{
		Logger::Log("Parameter validation failed: %s is null or empty.", paramName);
		return false;
	}
	return true;
}

// 从 "Namespace.ClassName" 格式中提取命名空间和类名
void ExtractNamespaceAndClass(const std::string& fullName, std::string& outNamespace, std::string& outClass)
{
	size_t lastDot = fullName.find_last_of('.');
	if (lastDot != std::string::npos)
	{
		outNamespace = fullName.substr(0, lastDot);
		outClass = fullName.substr(lastDot + 1);
	}
	else
	{
		outNamespace = "";
		outClass = fullName;
	}
}

// 加载 Mono 函数
bool LoadMonoFunctions()
{
	HMODULE hMono = GetModuleHandleW(L"mono-2.0-sgen.dll");
	if (!hMono)
	{
		Logger::Log("mono-2.0-sgen.dll is not loaded in this process.");
		return false;
	}
	Logger::Log("Found mono-2.0-sgen.dll module.");

	// 获取函数地址
	g_mono_get_root_domain = (FnGetRootDomain)GetProcAddress(hMono, "mono_get_root_domain");
	g_mono_domain_assembly_open = (FnMonoDomainAssemblyOpen)GetProcAddress(hMono, "mono_domain_assembly_open");
	g_mono_assembly_get_image = (FnMonoAssemblyGetImage)GetProcAddress(hMono, "mono_assembly_get_image");
	g_mono_class_from_name = (FnMonoClassFromName)GetProcAddress(hMono, "mono_class_from_name");
	g_mono_method_desc_new = (FnMonoMethodDescNew)GetProcAddress(hMono, "mono_method_desc_new");
	g_mono_method_desc_search_in_class = (FnMonoMethodDescSearchInClass)GetProcAddress(hMono, "mono_method_desc_search_in_class");
	g_mono_method_desc_free = (FnMonoMethodDescFree)GetProcAddress(hMono, "mono_method_desc_free");
	g_mono_string_new = (FnMonoStringNew)GetProcAddress(hMono, "mono_string_new");
	g_mono_runtime_invoke = (FnMonoRuntimeInvoke)GetProcAddress(hMono, "mono_runtime_invoke");
	g_mono_object_to_string = (FnMonoObjectToString)GetProcAddress(hMono, "mono_object_to_string");
	g_mono_string_to_utf8 = (FnMonoStringToUtf8)GetProcAddress(hMono, "mono_string_to_utf8");
	g_mono_free = (FnMonoFree)GetProcAddress(hMono, "mono_free");
	g_mono_method_signature = (FnMonoMethodSignature)GetProcAddress(hMono, "mono_method_signature");
	g_mono_signature_get_return_type = (FnMonoSignatureGetReturnType)GetProcAddress(hMono, "mono_signature_get_return_type");
	g_mono_type_get_type = (FnMonoTypeGetType)GetProcAddress(hMono, "mono_type_get_type");
	g_mono_thread_attach = (FnMonoThreadAttach)GetProcAddress(hMono, "mono_thread_attach");
	g_mono_object_unbox = (FnMonoObjectUnbox)GetProcAddress(hMono, "mono_object_unbox");

	// 验证所有函数指针
	if (!g_mono_get_root_domain) { Logger::Log("mono_get_root_domain not found!"); return false; }
	if (!g_mono_domain_assembly_open) { Logger::Log("mono_domain_assembly_open not found!"); return false; }
	if (!g_mono_assembly_get_image) { Logger::Log("mono_assembly_get_image not found!"); return false; }
	if (!g_mono_class_from_name) { Logger::Log("mono_class_from_name not found!"); return false; }
	if (!g_mono_method_desc_new) { Logger::Log("mono_method_desc_new not found!"); return false; }
	if (!g_mono_method_desc_search_in_class) { Logger::Log("mono_method_desc_search_in_class not found!"); return false; }
	if (!g_mono_method_desc_free) { Logger::Log("mono_method_desc_free not found!"); return false; }
	if (!g_mono_string_new) { Logger::Log("mono_string_new not found!"); return false; }
	if (!g_mono_runtime_invoke) { Logger::Log("mono_runtime_invoke not found!"); return false; }
	if (!g_mono_object_to_string) { Logger::Log("mono_object_to_string not found!"); return false; }
	if (!g_mono_string_to_utf8) { Logger::Log("mono_string_to_utf8 not found!"); return false; }
	if (!g_mono_free) { Logger::Log("mono_free not found!"); return false; }
	if (!g_mono_method_signature) { Logger::Log("mono_method_signature not found!"); return false; }
	if (!g_mono_signature_get_return_type) { Logger::Log("mono_signature_get_return_type not found!"); return false; }
	if (!g_mono_type_get_type) { Logger::Log("mono_type_get_type not found!"); return false; }
	if (!g_mono_thread_attach) { Logger::Log("mono_thread_attach not found!"); return false; }
	if (!g_mono_object_unbox) { Logger::Log("mono_object_unbox not found!"); return false; }

	Logger::Log("All required Mono functions loaded successfully.");
	return true;
}