#pragma once

#include <Windows.h>
#include <wincrypt.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "string_converter.h"

#pragma comment(lib, "Crypt32.lib")

class InjectionParameters {
private:
    std::wstring framework_version_;
    std::wstring assembly_file_;
    std::wstring entry_class_;
    std::wstring entry_method_;
    std::wstring entry_argument_;
    std::filesystem::path request_file_path_;

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
        std::unordered_map<std::string, std::string>& entries) {
        std::ifstream input_stream(request_file_path, std::ios::binary);
        if (!input_stream.is_open()) {
            return false;
        }

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
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
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
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
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
        close();

        request_file_path_ = get_request_file_path(process_id);
        std::unordered_map<std::string, std::string> entries;
        if (!try_read_entries(request_file_path_, entries)) {
            return false;
        }

        const auto version_entry = entries.find("format_version");
        if (version_entry == entries.end() || version_entry->second != "1") {
            close();
            return false;
        }

        const auto process_id_entry = entries.find("process_id");
        if (process_id_entry == entries.end() || process_id_entry->second != std::to_string(process_id)) {
            close();
            return false;
        }

        framework_version_ = get_decoded_value(entries, "runtime_hint");
        assembly_file_ = get_decoded_value(entries, "assembly_path");
        entry_class_ = get_decoded_value(entries, "entry_class");
        entry_method_ = get_decoded_value(entries, "entry_method");
        entry_argument_ = get_decoded_value(entries, "entry_argument");

        return are_all_required_params_set();
    }

    void close() {
        framework_version_.clear();
        assembly_file_.clear();
        entry_class_.clear();
        entry_method_.clear();
        entry_argument_.clear();
        request_file_path_.clear();
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