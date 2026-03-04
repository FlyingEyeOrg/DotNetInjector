#pragma once

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