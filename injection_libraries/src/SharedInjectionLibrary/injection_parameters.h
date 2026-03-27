#pragma once

#include <string>

#include "shared_memory.h"

class InjectionParameters {
public:
    struct Config {
        size_t max_framework_version_len = 128;
        size_t max_assembly_file_len = 512;
        size_t max_entry_class_len = 128;
        size_t max_entry_method_len = 128;
        size_t max_entry_arg_len = 512;
    };

private:
    Config config_{};
    SharedMemory framework_version_;
    SharedMemory assembly_file_;
    SharedMemory entry_class_;
    SharedMemory entry_method_;
    SharedMemory entry_argument_;

    std::wstring get_parameter(const SharedMemory& shared_memory) const {
        return shared_memory.is_valid() ? shared_memory.read() : L"";
    }

public:
    InjectionParameters() = default;

    ~InjectionParameters() {
        close();
    }

    bool open(const Config& config = Config()) {
        try {
            config_ = config;

            framework_version_.open(L"FrameworkVersion", sizeof(wchar_t) * config_.max_framework_version_len);
            entry_argument_.open(L"EntryArgument", sizeof(wchar_t) * config_.max_entry_arg_len);

            const bool success =
                assembly_file_.open(L"AssemblyFile", sizeof(wchar_t) * config_.max_assembly_file_len) &&
                entry_class_.open(L"EntryClass", sizeof(wchar_t) * config_.max_entry_class_len) &&
                entry_method_.open(L"EntryMethod", sizeof(wchar_t) * config_.max_entry_method_len);

            if (!success) {
                close();
                return false;
            }

            return true;
        } catch (...) {
            close();
            return false;
        }
    }

    void close() {
        framework_version_.close();
        assembly_file_.close();
        entry_class_.close();
        entry_method_.close();
        entry_argument_.close();
    }

    std::wstring get_framework_version() const {
        return get_parameter(framework_version_);
    }

    bool has_framework_version() const {
        return !get_framework_version().empty();
    }

    bool has_entry_argument() const {
        return !get_entry_argument().empty();
    }

    std::wstring get_assembly_file() const {
        return get_parameter(assembly_file_);
    }

    std::wstring get_entry_class() const {
        return get_parameter(entry_class_);
    }

    std::wstring get_entry_method() const {
        return get_parameter(entry_method_);
    }

    std::wstring get_entry_argument() const {
        return get_parameter(entry_argument_);
    }

    bool are_all_required_params_set() const {
        return !get_assembly_file().empty() &&
            !get_entry_class().empty() &&
            !get_entry_method().empty();
    }

    bool is_valid() const {
        return assembly_file_.is_valid() &&
            entry_class_.is_valid() &&
            entry_method_.is_valid();
    }

    std::wstring get_debug_summary() const {
        std::wstring summary;
        summary += L"FrameworkVersion: " + get_framework_version() + L" (optional)\n";
        summary += L"AssemblyFile: " + get_assembly_file() + L" (required)\n";
        summary += L"EntryClass: " + get_entry_class() + L" (required)\n";
        summary += L"EntryMethod: " + get_entry_method() + L" (required)\n";
        summary += L"EntryArgument: " + get_entry_argument() + L" (optional)\n";
        return summary;
    }
};