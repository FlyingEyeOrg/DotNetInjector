#include "pch.h"

#include <metahost.h>
#include <mscoree.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "injection_parameters.h"
#include "logger.h"
#include "managed_runtime_resolver.h"
#include "string_converter.h"

#pragma comment(lib, "mscoree.lib")

namespace {

using FnGetClrRuntimeHost = HRESULT(STDAPICALLTYPE*)(REFIID riid, IUnknown** p_unk);
using FnClrCreateInstance = HRESULT(WINAPI*)(REFCLSID clsid, REFIID riid, void** instance);

using MonoBool = int;
using MonoTypeEnum = int;

constexpr MonoTypeEnum k_mono_type_i4 = 0x08;

struct MonoDomain;
struct MonoAssembly;
struct MonoImage;
struct MonoClass;
struct MonoMethodDesc;
struct MonoMethod;
struct MonoString;
struct MonoObject;
struct MonoMethodSignature;
struct MonoType;
struct MonoThread;

using FnGetRootDomain = MonoDomain*(__cdecl*)();
using FnMonoDomainAssemblyOpen = MonoAssembly*(__cdecl*)(MonoDomain* domain, const char* name);
using FnMonoAssemblyGetImage = MonoImage*(__cdecl*)(MonoAssembly* assembly);
using FnMonoClassFromName = MonoClass*(__cdecl*)(MonoImage* image, const char* namesp, const char* name);
using FnMonoMethodDescNew = MonoMethodDesc*(__cdecl*)(const char* name, MonoBool include_namespace);
using FnMonoMethodDescSearchInClass = MonoMethod*(__cdecl*)(MonoMethodDesc* desc, MonoClass* klass);
using FnMonoMethodDescFree = void(__cdecl*)(MonoMethodDesc* desc);
using FnMonoStringNew = MonoString*(__cdecl*)(MonoDomain* domain, const char* text);
using FnMonoRuntimeInvoke = MonoObject*(__cdecl*)(MonoMethod* method, void* instance, void** params, MonoObject** exc);
using FnMonoObjectToString = MonoString*(__cdecl*)(MonoObject* obj, MonoObject** exc);
using FnMonoStringToUtf8 = char*(__cdecl*)(MonoString* s);
using FnMonoFree = void(__cdecl*)(void* ptr);
using FnMonoMethodSignature = MonoMethodSignature*(__cdecl*)(MonoMethod* method);
using FnMonoSignatureGetReturnType = MonoType*(__cdecl*)(MonoMethodSignature* sig);
using FnMonoTypeGetType = MonoTypeEnum(__cdecl*)(MonoType* type);
using FnMonoThreadAttach = MonoThread*(__cdecl*)(MonoDomain* domain);
using FnMonoObjectUnbox = void*(__cdecl*)(MonoObject* obj);

struct MonoApi {
    FnGetRootDomain mono_get_root_domain{};
    FnMonoDomainAssemblyOpen mono_domain_assembly_open{};
    FnMonoAssemblyGetImage mono_assembly_get_image{};
    FnMonoClassFromName mono_class_from_name{};
    FnMonoMethodDescNew mono_method_desc_new{};
    FnMonoMethodDescSearchInClass mono_method_desc_search_in_class{};
    FnMonoMethodDescFree mono_method_desc_free{};
    FnMonoStringNew mono_string_new{};
    FnMonoRuntimeInvoke mono_runtime_invoke{};
    FnMonoObjectToString mono_object_to_string{};
    FnMonoStringToUtf8 mono_string_to_utf8{};
    FnMonoFree mono_free{};
    FnMonoMethodSignature mono_method_signature{};
    FnMonoSignatureGetReturnType mono_signature_get_return_type{};
    FnMonoTypeGetType mono_type_get_type{};
    FnMonoThreadAttach mono_thread_attach{};
    FnMonoObjectUnbox mono_object_unbox{};
};

struct MonoResolvedMethod {
    MonoMethod* method{};
    bool takes_argument{};
};

ICLRRuntimeHost* framework_runtime_host = nullptr;
bool framework_runtime_initialized = false;

bool validate_parameters(const InjectionParameters& parameters) {
    const auto assembly_file = parameters.get_assembly_file();
    const auto entry_class = parameters.get_entry_class();
    const auto entry_method = parameters.get_entry_method();

    if (assembly_file.empty()) {
        Logger::log("Error: Assembly file path is empty.");
        return false;
    }

    if (entry_class.empty()) {
        Logger::log("Error: Entry class name is empty.");
        return false;
    }

    if (entry_method.empty()) {
        Logger::log("Error: Entry method name is empty.");
        return false;
    }

    const std::filesystem::path assembly_path(assembly_file);
    if (!std::filesystem::exists(assembly_path)) {
        Logger::log("Error: Assembly file does not exist: %s",
                StringConverter::wstring_to_gbk(assembly_file).c_str());
        return false;
    }

    if (assembly_path.extension() != L".dll") {
        Logger::log("Error: Assembly file is not a DLL: %s",
                StringConverter::wstring_to_gbk(assembly_file).c_str());
        return false;
    }

    return true;
}

void log_parameters(const InjectionParameters& parameters) {
    Logger::log("Executing managed method...");
    Logger::log("  Assembly: %s",
                StringConverter::wstring_to_gbk(parameters.get_assembly_file()).c_str());
    Logger::log("  Class:    %s",
                StringConverter::wstring_to_gbk(parameters.get_entry_class()).c_str());
    Logger::log("  Method:   %s",
                StringConverter::wstring_to_gbk(parameters.get_entry_method()).c_str());
    Logger::log("  Argument: %s",
                StringConverter::wstring_to_gbk(parameters.get_entry_argument()).c_str());
}

void log_hresult(HRESULT hr, const char* context) {
    Logger::log("%s failed. HRESULT=0x%08X (%s)", context, hr,
                std::system_category().message(hr).c_str());
}

bool try_get_coreclr_runtime_host(ICLRRuntimeHost** runtime_host) {
    HMODULE coreclr_module = ::GetModuleHandleW(L"coreclr.dll");
    if (!coreclr_module) {
        coreclr_module = ::GetModuleHandleW(L"coreclr.so");
    }
    if (!coreclr_module) {
        coreclr_module = ::GetModuleHandleW(L"coreclr.dynlib");
    }
    if (!coreclr_module) {
        Logger::log("coreclr runtime module is not loaded.");
        return false;
    }

    auto get_runtime_host = reinterpret_cast<FnGetClrRuntimeHost>(
        ::GetProcAddress(coreclr_module, "GetCLRRuntimeHost"));
    if (!get_runtime_host) {
        Logger::log("GetCLRRuntimeHost export was not found.");
        return false;
    }

    HRESULT hr = get_runtime_host(IID_ICLRRuntimeHost,
                                  reinterpret_cast<IUnknown**>(runtime_host));
    if (FAILED(hr) || *runtime_host == nullptr) {
        log_hresult(hr, "GetCLRRuntimeHost");
        return false;
    }

    return true;
}

bool execute_via_coreclr(const InjectionParameters& parameters) {
    ICLRRuntimeHost* runtime_host = nullptr;
    if (!try_get_coreclr_runtime_host(&runtime_host)) {
        return false;
    }

    DWORD return_value = 0;
    const HRESULT hr = runtime_host->ExecuteInDefaultAppDomain(
        parameters.get_assembly_file().c_str(),
        parameters.get_entry_class().c_str(),
        parameters.get_entry_method().c_str(),
        parameters.get_entry_argument().c_str(),
        &return_value);

    if (FAILED(hr)) {
        log_hresult(hr, "CoreCLR ExecuteInDefaultAppDomain");
        runtime_host->Release();
        return false;
    }

    Logger::log("CoreCLR managed method executed successfully. Return value=%lu",
                return_value);
    runtime_host->Release();
    return true;
}

bool try_initialize_framework_host(const InjectionParameters& parameters,
                                   ICLRRuntimeHost** runtime_host) {
    if (framework_runtime_initialized && framework_runtime_host != nullptr) {
        *runtime_host = framework_runtime_host;
        return true;
    }

    HMODULE mscoree_module = ::GetModuleHandleW(L"mscoree.dll");
    if (!mscoree_module) {
        Logger::log("mscoree.dll not found in process.");
        return false;
    }

    auto clr_create_instance = reinterpret_cast<FnClrCreateInstance>(
        ::GetProcAddress(mscoree_module, "CLRCreateInstance"));
    if (!clr_create_instance) {
        Logger::log("CLRCreateInstance export was not found.");
        return false;
    }

    ICLRMetaHost* meta_host = nullptr;
    HRESULT hr = clr_create_instance(CLSID_CLRMetaHost, IID_PPV_ARGS(&meta_host));
    if (FAILED(hr) || meta_host == nullptr) {
        log_hresult(hr, "CLRCreateInstance");
        return false;
    }

    const auto configured_version = parameters.get_framework_version();
    const auto framework_version = configured_version.empty() ? L"v4.0.30319" : configured_version;

    ICLRRuntimeInfo* runtime_info = nullptr;
    hr = meta_host->GetRuntime(framework_version.c_str(), IID_PPV_ARGS(&runtime_info));
    if (FAILED(hr) || runtime_info == nullptr) {
        log_hresult(hr, "ICLRMetaHost::GetRuntime");
        meta_host->Release();
        return false;
    }

    hr = runtime_info->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(runtime_host));
    if (FAILED(hr) || *runtime_host == nullptr) {
        log_hresult(hr, "ICLRRuntimeInfo::GetInterface");
        runtime_info->Release();
        meta_host->Release();
        return false;
    }

    framework_runtime_host = *runtime_host;
    framework_runtime_initialized = true;

    runtime_info->Release();
    meta_host->Release();
    return true;
}

bool execute_via_framework(const InjectionParameters& parameters) {
    ICLRRuntimeHost* runtime_host = nullptr;
    if (!try_initialize_framework_host(parameters, &runtime_host)) {
        return false;
    }

    DWORD return_value = 0;
    const HRESULT hr = runtime_host->ExecuteInDefaultAppDomain(
        parameters.get_assembly_file().c_str(),
        parameters.get_entry_class().c_str(),
        parameters.get_entry_method().c_str(),
        parameters.get_entry_argument().c_str(),
        &return_value);

    if (FAILED(hr)) {
        log_hresult(hr, "Framework ExecuteInDefaultAppDomain");
        return false;
    }

    Logger::log("Framework managed method executed successfully. Return value=%lu",
                return_value);
    return true;
}

void extract_namespace_and_class(const std::string& full_name,
                                 std::string& namespace_name,
                                 std::string& class_name) {
    const auto last_dot = full_name.find_last_of('.');
    if (last_dot == std::string::npos) {
        namespace_name.clear();
        class_name = full_name;
        return;
    }

    namespace_name = full_name.substr(0, last_dot);
    class_name = full_name.substr(last_dot + 1);
}

bool load_mono_api(MonoApi& api) {
    HMODULE mono_module = ::GetModuleHandleW(L"mono-2.0-sgen.dll");
    if (!mono_module) {
        mono_module = ::GetModuleHandleW(L"mono-2.0-bdwgc.dll");
    }
    if (!mono_module) {
        Logger::log("mono runtime module is not loaded in this process.");
        return false;
    }

    api.mono_get_root_domain = reinterpret_cast<FnGetRootDomain>(
        ::GetProcAddress(mono_module, "mono_get_root_domain"));
    api.mono_domain_assembly_open = reinterpret_cast<FnMonoDomainAssemblyOpen>(
        ::GetProcAddress(mono_module, "mono_domain_assembly_open"));
    api.mono_assembly_get_image = reinterpret_cast<FnMonoAssemblyGetImage>(
        ::GetProcAddress(mono_module, "mono_assembly_get_image"));
    api.mono_class_from_name = reinterpret_cast<FnMonoClassFromName>(
        ::GetProcAddress(mono_module, "mono_class_from_name"));
    api.mono_method_desc_new = reinterpret_cast<FnMonoMethodDescNew>(
        ::GetProcAddress(mono_module, "mono_method_desc_new"));
    api.mono_method_desc_search_in_class = reinterpret_cast<FnMonoMethodDescSearchInClass>(
        ::GetProcAddress(mono_module, "mono_method_desc_search_in_class"));
    api.mono_method_desc_free = reinterpret_cast<FnMonoMethodDescFree>(
        ::GetProcAddress(mono_module, "mono_method_desc_free"));
    api.mono_string_new = reinterpret_cast<FnMonoStringNew>(
        ::GetProcAddress(mono_module, "mono_string_new"));
    api.mono_runtime_invoke = reinterpret_cast<FnMonoRuntimeInvoke>(
        ::GetProcAddress(mono_module, "mono_runtime_invoke"));
    api.mono_object_to_string = reinterpret_cast<FnMonoObjectToString>(
        ::GetProcAddress(mono_module, "mono_object_to_string"));
    api.mono_string_to_utf8 = reinterpret_cast<FnMonoStringToUtf8>(
        ::GetProcAddress(mono_module, "mono_string_to_utf8"));
    api.mono_free = reinterpret_cast<FnMonoFree>(
        ::GetProcAddress(mono_module, "mono_free"));
    api.mono_method_signature = reinterpret_cast<FnMonoMethodSignature>(
        ::GetProcAddress(mono_module, "mono_method_signature"));
    api.mono_signature_get_return_type = reinterpret_cast<FnMonoSignatureGetReturnType>(
        ::GetProcAddress(mono_module, "mono_signature_get_return_type"));
    api.mono_type_get_type = reinterpret_cast<FnMonoTypeGetType>(
        ::GetProcAddress(mono_module, "mono_type_get_type"));
    api.mono_thread_attach = reinterpret_cast<FnMonoThreadAttach>(
        ::GetProcAddress(mono_module, "mono_thread_attach"));
    api.mono_object_unbox = reinterpret_cast<FnMonoObjectUnbox>(
        ::GetProcAddress(mono_module, "mono_object_unbox"));

    return api.mono_get_root_domain && api.mono_domain_assembly_open &&
           api.mono_assembly_get_image && api.mono_class_from_name &&
           api.mono_method_desc_new && api.mono_method_desc_search_in_class &&
           api.mono_method_desc_free && api.mono_string_new &&
           api.mono_runtime_invoke && api.mono_object_to_string &&
           api.mono_string_to_utf8 && api.mono_free &&
           api.mono_method_signature && api.mono_signature_get_return_type &&
           api.mono_type_get_type && api.mono_thread_attach &&
           api.mono_object_unbox;
}

MonoResolvedMethod resolve_mono_method(
    const MonoApi& api,
    MonoClass* klass,
    const std::string& entry_class_utf8,
    const std::string& entry_method_utf8,
    bool include_namespace) {
    const std::string method_descriptors[] = {
        entry_class_utf8 + "::" + entry_method_utf8 + "(string)",
        entry_class_utf8 + "::" + entry_method_utf8 + "()",
    };

    for (size_t index = 0; index < std::size(method_descriptors); ++index) {
        auto* method_desc = api.mono_method_desc_new(method_descriptors[index].c_str(), include_namespace ? TRUE : FALSE);
        if (!method_desc) {
            continue;
        }

        auto* method = api.mono_method_desc_search_in_class(method_desc, klass);
        api.mono_method_desc_free(method_desc);
        if (method) {
            return MonoResolvedMethod{method, index == 0};
        }
    }

    return MonoResolvedMethod{};
}

bool execute_via_mono(const InjectionParameters& parameters) {
    MonoApi api{};
    if (!load_mono_api(api)) {
        Logger::log("Failed to load required Mono exports.");
        return false;
    }

    auto* domain = api.mono_get_root_domain();
    if (!domain) {
        Logger::log("mono_get_root_domain returned nullptr.");
        return false;
    }

    if (!api.mono_thread_attach(domain)) {
        Logger::log("mono_thread_attach failed.");
        return false;
    }

    const auto assembly_file_utf8 =
        StringConverter::wstring_to_utf8(parameters.get_assembly_file());
    const auto entry_class_utf8 =
        StringConverter::wstring_to_utf8(parameters.get_entry_class());
    const auto entry_method_utf8 =
        StringConverter::wstring_to_utf8(parameters.get_entry_method());
    const auto entry_argument_utf8 =
        StringConverter::wstring_to_utf8(parameters.get_entry_argument());

    auto* assembly = api.mono_domain_assembly_open(domain, assembly_file_utf8.c_str());
    if (!assembly) {
        Logger::log("Failed to open Mono assembly: %s", assembly_file_utf8.c_str());
        return false;
    }

    auto* image = api.mono_assembly_get_image(assembly);
    if (!image) {
        Logger::log("Failed to get Mono image.");
        return false;
    }

    std::string namespace_name;
    std::string class_name;
    extract_namespace_and_class(entry_class_utf8, namespace_name, class_name);

    auto* klass = api.mono_class_from_name(image, namespace_name.c_str(), class_name.c_str());
    if (!klass) {
        Logger::log("Mono class was not found: %s.%s", namespace_name.c_str(), class_name.c_str());
        return false;
    }

    const auto resolved_method = resolve_mono_method(
        api,
        klass,
        entry_class_utf8,
        entry_method_utf8,
        !namespace_name.empty());
    if (!resolved_method.method) {
        Logger::log("Mono method was not found: %s", entry_method_utf8.c_str());
        return false;
    }

    MonoString* argument = nullptr;
    void* args[1] = {};
    void** invocation_args = nullptr;
    if (resolved_method.takes_argument) {
        argument = api.mono_string_new(domain, entry_argument_utf8.c_str());
        if (!argument) {
            Logger::log("Failed to allocate Mono argument string.");
            return false;
        }

        args[0] = argument;
        invocation_args = args;
    }

    MonoObject* exception = nullptr;
    MonoObject* result = api.mono_runtime_invoke(resolved_method.method, nullptr, invocation_args, &exception);
    if (exception) {
        auto* exception_text = api.mono_object_to_string(exception, nullptr);
        if (exception_text) {
            char* utf8_exception = api.mono_string_to_utf8(exception_text);
            if (utf8_exception) {
                Logger::log("Mono invocation exception: %s", utf8_exception);
                api.mono_free(utf8_exception);
            }
        }
        return false;
    }

    auto* method_signature = api.mono_method_signature(resolved_method.method);
    if (!method_signature) {
        Logger::log("Failed to query Mono method signature.");
        return false;
    }

    auto* return_type = api.mono_signature_get_return_type(method_signature);
    if (!return_type || api.mono_type_get_type(return_type) != k_mono_type_i4) {
        Logger::log("Mono method return type is not int.");
        return false;
    }

    if (!result) {
        Logger::log("Mono method returned nullptr.");
        return false;
    }

    auto* unboxed_result = api.mono_object_unbox(result);
    if (!unboxed_result) {
        Logger::log("Mono method returned a null boxed result.");
        return false;
    }

    const int return_value = *reinterpret_cast<int*>(unboxed_result);
    Logger::log("Mono managed method executed successfully. Return value=%d",
                return_value);
    return true;
}

bool dispatch_runtime(const InjectionParameters& parameters) {
    const auto runtime_presence = detect_runtime_presence();
    const auto runtime_kind = resolve_runtime_kind(parameters.get_framework_version(),
                                                   runtime_presence);

    Logger::log("Resolved runtime kind: %s", to_string(runtime_kind));

    switch (runtime_kind) {
        case ManagedRuntimeKind::dotnet_framework:
            return execute_via_framework(parameters);
        case ManagedRuntimeKind::dotnet:
            return execute_via_coreclr(parameters);
        case ManagedRuntimeKind::mono:
            return execute_via_mono(parameters);
        default:
            Logger::log("Unable to resolve target managed runtime.");
            return false;
    }
}

DWORD WINAPI execute_managed_injection(LPVOID parameter) {
    Logger::initialize();

    const auto module = static_cast<HMODULE>(parameter);
    Logger::log("Managed injection worker thread started. Module=%p", module);
    wchar_t module_file_path[MAX_PATH] = L"";
    std::optional<std::filesystem::path> module_directory;
    const DWORD module_path_length = ::GetModuleFileNameW(module, module_file_path, MAX_PATH);
    if (module_path_length > 0 && module_path_length < MAX_PATH) {
        module_directory = std::filesystem::path(module_file_path).parent_path();
    }

    InjectionParameters parameters;
    const DWORD current_process_id = ::GetCurrentProcessId();
    if (!parameters.load_for_process_id(current_process_id, module_directory)) {
        Logger::log(
            "Failed to load injection parameters. CurrentProcessId=%lu, Reason=%s, CandidatePaths:\n%s",
            current_process_id,
            StringConverter::wstring_to_gbk(parameters.get_load_failure_reason()).c_str(),
            StringConverter::wstring_to_gbk(parameters.get_request_lookup_summary()).c_str());
        return 1;
    }

    Logger::log("Opened injection parameter request file.");
    Logger::log("Injection parameters:\n%s",
                StringConverter::wstring_to_gbk(parameters.get_debug_summary()).c_str());

    if (!validate_parameters(parameters)) {
        return 2;
    }

    log_parameters(parameters);
    if (!dispatch_runtime(parameters)) {
        return 3;
    }

    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(module);

            const HANDLE worker_thread = ::CreateThread(
                nullptr,
                0,
                &execute_managed_injection,
                module,
                0,
                nullptr);
            if (!worker_thread) {
                Logger::initialize();
                Logger::log("Failed to create managed injection worker thread. LastError=%lu", ::GetLastError());
                return FALSE;
            }

            ::CloseHandle(worker_thread);
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}