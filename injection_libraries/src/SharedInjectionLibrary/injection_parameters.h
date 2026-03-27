#pragma once

#include <Windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "string_converter.h"

#pragma comment(lib, "Crypt32.lib")

class InjectionParameters {
private:
    static constexpr DWORD k_request_file_retry_window_ms = 5000;
    static constexpr DWORD k_request_file_retry_interval_ms = 50;
    static constexpr DWORD k_request_file_retry_interval_on_lock_ms = 10;

    std::wstring framework_version_;
    std::wstring assembly_file_;
    std::wstring entry_class_;
    std::wstring entry_method_;
    std::wstring entry_argument_;
    std::filesystem::path request_file_path_;
    std::vector<std::filesystem::path> attempted_request_file_paths_;
    std::vector<std::pair<std::filesystem::path, DWORD>> attempted_request_file_errors_;
    std::wstring load_failure_reason_;

    static std::filesystem::path get_request_directory() {
        wchar_t temp_path[MAX_PATH] = L"";
        const DWORD length = ::GetTempPathW(MAX_PATH, temp_path);
        if (length == 0 || length >= MAX_PATH) {
            return std::filesystem::current_path() / L"DotNetInjector" / L"requests";
        }

        return std::filesystem::path(temp_path) / L"DotNetInjector" / L"requests";
    }

    static std::filesystem::path get_request_file_path(DWORD process_id) {
        return get_request_directory() / (L"request-" + std::to_wstring(process_id) + L".txt");
    }

    static std::filesystem::path get_request_file_path(
        DWORD process_id,
        const std::filesystem::path& base_directory) {
        if (base_directory.empty()) {
            return get_request_file_path(process_id);
        }

        return base_directory / (L"request-" + std::to_wstring(process_id) + L".txt");
    }

    static std::filesystem::path get_module_request_file_path(
        const std::filesystem::path& base_directory) {
        return base_directory / L"request.txt";
    }

    static std::string trim(const std::string& value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static bool try_read_entries(
        const std::filesystem::path& request_file_path,
        DWORD& last_error,
        std::unordered_map<std::string, std::string>& entries) {
        const HANDLE file_handle = ::CreateFileW(
            request_file_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file_handle == INVALID_HANDLE_VALUE) {
            last_error = ::GetLastError();
            return false;
        }

        last_error = ERROR_SUCCESS;

        LARGE_INTEGER file_size{};
        if (!::GetFileSizeEx(file_handle, &file_size) || file_size.QuadPart < 0) {
            last_error = ::GetLastError();
            ::CloseHandle(file_handle);
            return false;
        }

        if (file_size.QuadPart == 0) {
            last_error = ERROR_RETRY;
            ::CloseHandle(file_handle);
            return false;
        }

        std::string content(static_cast<size_t>(file_size.QuadPart), '\0');
        DWORD total_bytes_read = 0;
        while (total_bytes_read < content.size()) {
            DWORD bytes_read = 0;
            const BOOL read_result = ::ReadFile(
                file_handle,
                content.data() + total_bytes_read,
                static_cast<DWORD>(content.size() - total_bytes_read),
                &bytes_read,
                nullptr);
            if (!read_result) {
                last_error = ::GetLastError();
                ::CloseHandle(file_handle);
                return false;
            }

            if (bytes_read == 0) {
                break;
            }

            total_bytes_read += bytes_read;
        }

        ::CloseHandle(file_handle);
        content.resize(total_bytes_read);

        std::istringstream input_stream(content);
        std::string line;
        while (std::getline(input_stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const auto separator_index = line.find('=');
            if (separator_index == std::string::npos) {
                continue;
            }

            const auto key = trim(line.substr(0, separator_index));
            const auto value = trim(line.substr(separator_index + 1));
            if (!key.empty()) {
                entries[key] = value;
            }
        }

        return true;
    }

    static std::string decode_base64(const std::string& value) {
        if (value.empty()) {
            return {};
        }

        DWORD required_size = 0;
        if (!::CryptStringToBinaryA(
                value.c_str(),
                static_cast<DWORD>(value.size()),
            CRYPT_STRING_BASE64_ANY,
                nullptr,
                &required_size,
                nullptr,
                nullptr)) {
            return {};
        }

        std::string decoded(required_size, '\0');
        if (!::CryptStringToBinaryA(
                value.c_str(),
                static_cast<DWORD>(value.size()),
            CRYPT_STRING_BASE64_ANY,
                reinterpret_cast<BYTE*>(decoded.data()),
                &required_size,
                nullptr,
                nullptr)) {
            return {};
        }

        decoded.resize(required_size);
        return decoded;
    }

    static std::wstring get_decoded_value(
        const std::unordered_map<std::string, std::string>& entries,
        const char* key) {
        const auto entry = entries.find(key);
        if (entry == entries.end()) {
            return L"";
        }

        return StringConverter::utf8_to_wstring(decode_base64(entry->second));
    }

public:
    bool open() {
        return load_for_process_id(::GetCurrentProcessId());
    }

    bool load_for_process_id(DWORD process_id) {
        return load_for_process_id(process_id, std::nullopt);
    }

    bool load_for_process_id(
        DWORD process_id,
        const std::optional<std::filesystem::path>& module_directory) {
        attempted_request_file_paths_.clear();
        attempted_request_file_errors_.clear();
        load_failure_reason_.clear();
        close();

        std::vector<std::filesystem::path> request_file_paths;
        request_file_paths.push_back(get_request_file_path(process_id));

        if (module_directory.has_value() && !module_directory->empty()) {
            const auto module_request_file_path = get_request_file_path(process_id, *module_directory);
            if (module_request_file_path != request_file_paths.front()) {
                request_file_paths.push_back(module_request_file_path);
            }

            const auto module_fallback_request_file_path = get_module_request_file_path(*module_directory);
            if (module_fallback_request_file_path != request_file_paths.front() &&
                module_fallback_request_file_path != module_request_file_path) {
                request_file_paths.push_back(module_fallback_request_file_path);
            }
        }

        attempted_request_file_paths_ = request_file_paths;

        std::unordered_map<std::string, std::string> entries;
        bool request_file_found = false;
        const ULONGLONG started_at = ::GetTickCount64();
        while (!request_file_found && (::GetTickCount64() - started_at) <= k_request_file_retry_window_ms) {
            DWORD retry_delay_ms = k_request_file_retry_interval_ms;
            for (const auto& candidate_request_file_path : request_file_paths) {
                entries.clear();
                DWORD last_error = ERROR_SUCCESS;
                if (!try_read_entries(candidate_request_file_path, last_error, entries)) {
                    if (last_error == ERROR_SHARING_VIOLATION || last_error == ERROR_LOCK_VIOLATION) {
                        retry_delay_ms = k_request_file_retry_interval_on_lock_ms;
                    }

                    auto existing_error = std::find_if(
                        attempted_request_file_errors_.begin(),
                        attempted_request_file_errors_.end(),
                        [&candidate_request_file_path](const auto& item) {
                            return item.first == candidate_request_file_path;
                        });

                    if (existing_error == attempted_request_file_errors_.end()) {
                        attempted_request_file_errors_.emplace_back(candidate_request_file_path, last_error);
                    } else {
                        existing_error->second = last_error;
                    }

                    continue;
                }

                request_file_path_ = candidate_request_file_path;
                request_file_found = true;
                break;
            }

            if (!request_file_found) {
                ::Sleep(retry_delay_ms);
            }
        }

        if (!request_file_found) {
            load_failure_reason_ = L"request-file-not-found";
            close();
            return false;
        }

        const auto version_entry = entries.find("format_version");
        if (version_entry == entries.end() || version_entry->second != "1") {
            load_failure_reason_ = L"invalid-format-version";
            close();
            return false;
        }

        const auto process_id_entry = entries.find("process_id");
        if (process_id_entry == entries.end() || process_id_entry->second != std::to_string(process_id)) {
            load_failure_reason_ = L"process-id-mismatch";
            close();
            return false;
        }

        framework_version_ = get_decoded_value(entries, "runtime_hint");
        assembly_file_ = get_decoded_value(entries, "assembly_path");
        entry_class_ = get_decoded_value(entries, "entry_class");
        entry_method_ = get_decoded_value(entries, "entry_method");
        entry_argument_ = get_decoded_value(entries, "entry_argument");

        const bool all_required_params_set = are_all_required_params_set();
        if (!all_required_params_set) {
            load_failure_reason_ = L"missing-required-parameters";
        }

        return all_required_params_set;
    }

    void close() {
        framework_version_.clear();
        assembly_file_.clear();
        entry_class_.clear();
        entry_method_.clear();
        entry_argument_.clear();
        request_file_path_.clear();
    }

    std::wstring get_request_lookup_summary() const {
        std::wstring summary;
        for (const auto& attempted_request_file_path : attempted_request_file_paths_) {
            if (!summary.empty()) {
                summary += L"\n";
            }

            summary += attempted_request_file_path.wstring();

            const auto error = std::find_if(
                attempted_request_file_errors_.begin(),
                attempted_request_file_errors_.end(),
                [&attempted_request_file_path](const auto& item) {
                    return item.first == attempted_request_file_path;
                });
            if (error != attempted_request_file_errors_.end()) {
                summary += L" (LastError=" + std::to_wstring(error->second) + L")";
            }
        }

        return summary;
    }

    std::wstring get_load_failure_reason() const {
        return load_failure_reason_;
    }

    std::wstring get_framework_version() const {
        return framework_version_;
    }

    bool has_framework_version() const {
        return !framework_version_.empty();
    }

    bool has_entry_argument() const {
        return !entry_argument_.empty();
    }

    std::wstring get_assembly_file() const {
        return assembly_file_;
    }

    std::wstring get_entry_class() const {
        return entry_class_;
    }

    std::wstring get_entry_method() const {
        return entry_method_;
    }

    std::wstring get_entry_argument() const {
        return entry_argument_;
    }

    bool are_all_required_params_set() const {
        return !assembly_file_.empty() && !entry_class_.empty() && !entry_method_.empty();
    }

    bool is_valid() const {
        return are_all_required_params_set();
    }

    std::wstring get_debug_summary() const {
        std::wstring summary;
        summary += L"RequestFile: " + request_file_path_.wstring() + L"\n";
        summary += L"FrameworkVersion: " + framework_version_ + L" (optional)\n";
        summary += L"AssemblyFile: " + assembly_file_ + L" (required)\n";
        summary += L"EntryClass: " + entry_class_ + L" (required)\n";
        summary += L"EntryMethod: " + entry_method_ + L" (required)\n";
        summary += L"EntryArgument: " + entry_argument_ + L" (optional)\n";
        return summary;
    }
};