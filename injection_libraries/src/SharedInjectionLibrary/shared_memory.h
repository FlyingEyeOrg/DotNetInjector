#pragma once

#include <windows.h>

#include <cwchar>
#include <string>

#include "framework.h"

class SharedMemory final {
private:
    static constexpr wchar_t k_guid[] = APP_ID;

    HANDLE map_file_handle_ = nullptr;
    wchar_t* data_ = nullptr;
    size_t buffer_size_bytes_ = 0;
    size_t string_length_ = 0;
    HANDLE mutex_handle_ = nullptr;
    wchar_t mutex_name_[256] = L"";

    static std::wstring make_unique_name(const wchar_t* base_name) {
        std::wstring unique_name = L"Global\\[";
        unique_name += k_guid;
        unique_name += L"]-";
        unique_name += base_name;
        return unique_name;
    }

    size_t buffer_size_chars() const noexcept {
        return buffer_size_bytes_ / sizeof(wchar_t);
    }

    bool wait_for_mutex() const noexcept {
        const DWORD wait_result = ::WaitForSingleObject(mutex_handle_, INFINITE);
        return wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED;
    }

    void release_mutex() const noexcept {
        ::ReleaseMutex(mutex_handle_);
    }

public:
    SharedMemory() = default;

    ~SharedMemory() {
        close();
    }

    bool open(const wchar_t* base_name, size_t buffer_size_bytes) {
        const std::wstring unique_name = make_unique_name(base_name);

        buffer_size_bytes_ = buffer_size_bytes;
        string_length_ = 0;

        map_file_handle_ = ::OpenFileMappingW(FILE_MAP_READ, FALSE, unique_name.c_str());
        if (!map_file_handle_) {
            return false;
        }

        data_ = static_cast<wchar_t*>(::MapViewOfFile(map_file_handle_, FILE_MAP_READ, 0, 0, buffer_size_bytes));
        if (!data_) {
            ::CloseHandle(map_file_handle_);
            map_file_handle_ = nullptr;
            return false;
        }

        string_length_ = wcsnlen_s(data_, buffer_size_chars());

        swprintf_s(mutex_name_, L"Global\\[%s]-ProcessInjector_SharedMemory_Mutex_%s", k_guid, base_name);
        mutex_handle_ = ::OpenMutexW(MUTEX_ALL_ACCESS, FALSE, mutex_name_);
        if (!mutex_handle_) {
            close();
            return false;
        }

        return true;
    }

    void close() {
        if (data_) {
            ::UnmapViewOfFile(data_);
            data_ = nullptr;
        }
        if (map_file_handle_) {
            ::CloseHandle(map_file_handle_);
            map_file_handle_ = nullptr;
        }
        if (mutex_handle_) {
            ::CloseHandle(mutex_handle_);
            mutex_handle_ = nullptr;
        }
        buffer_size_bytes_ = 0;
        string_length_ = 0;
        mutex_name_[0] = L'\0';
    }

    bool is_valid() const noexcept {
        return data_ != nullptr && mutex_handle_ != nullptr;
    }

    bool write(const std::wstring& value) {
        if (!is_valid()) {
            return false;
        }

        const size_t value_length_with_null = value.length() + 1;
        if (value_length_with_null > buffer_size_chars()) {
            return false;
        }

        if (!wait_for_mutex()) {
            return false;
        }

        wcscpy_s(data_, buffer_size_chars(), value.c_str());
        string_length_ = value.length();
        release_mutex();
        return true;
    }

    bool write(const wchar_t* value) {
        return value != nullptr && write(std::wstring(value));
    }

    bool read(std::wstring& output) const {
        if (!is_valid()) {
            return false;
        }

        if (!wait_for_mutex()) {
            return false;
        }

        output.assign(data_, wcsnlen_s(data_, buffer_size_chars()));
        release_mutex();
        return true;
    }

    std::wstring read() const {
        std::wstring value;
        return read(value) ? value : L"";
    }

    bool append(const std::wstring& value) {
        if (!is_valid() || value.empty()) {
            return false;
        }

        const size_t new_total_length = string_length_ + value.length();
        if (new_total_length + 1 > buffer_size_chars()) {
            return false;
        }

        if (!wait_for_mutex()) {
            return false;
        }

        wcscat_s(data_, buffer_size_chars(), value.c_str());
        string_length_ = new_total_length;
        release_mutex();
        return true;
    }

    size_t get_string_length() const noexcept {
        return string_length_;
    }

    size_t get_buffer_size() const noexcept {
        return buffer_size_bytes_;
    }

    bool is_empty() const noexcept {
        return string_length_ == 0;
    }

    bool clear() {
        if (!is_valid()) {
            return false;
        }

        if (!wait_for_mutex()) {
            return false;
        }

        data_[0] = L'\0';
        string_length_ = 0;
        release_mutex();
        return true;
    }

    const wchar_t* get_data() const noexcept {
        return data_;
    }
};