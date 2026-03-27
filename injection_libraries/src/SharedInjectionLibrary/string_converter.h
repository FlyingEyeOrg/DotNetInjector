#pragma once

#include <Windows.h>

#include <string>
#include <vector>

class StringConverter final {
private:
    static std::wstring multi_byte_to_wstring(const std::string& value, UINT code_page) {
        if (value.empty()) {
            return L"";
        }

        const int size_needed = ::MultiByteToWideChar(
            code_page,
            0,
            value.c_str(),
            static_cast<int>(value.length()),
            nullptr,
            0);
        if (size_needed <= 0) {
            return L"";
        }

        std::vector<wchar_t> buffer(size_needed + 1);
        const int result = ::MultiByteToWideChar(
            code_page,
            0,
            value.c_str(),
            static_cast<int>(value.length()),
            buffer.data(),
            size_needed);
        if (result <= 0) {
            return L"";
        }

        buffer[result] = L'\0';
        return std::wstring(buffer.data());
    }

    static std::string wstring_to_multi_byte(const std::wstring& value, UINT code_page) {
        if (value.empty()) {
            return "";
        }

        const int size_needed = ::WideCharToMultiByte(
            code_page,
            0,
            value.c_str(),
            static_cast<int>(value.length()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (size_needed <= 0) {
            return "";
        }

        std::vector<char> buffer(size_needed + 1);
        const int result = ::WideCharToMultiByte(
            code_page,
            0,
            value.c_str(),
            static_cast<int>(value.length()),
            buffer.data(),
            size_needed,
            nullptr,
            nullptr);
        if (result <= 0) {
            return "";
        }

        buffer[result] = '\0';
        return std::string(buffer.data());
    }

public:
    static std::string gbk_to_utf8(const std::string& gbk_str) {
        return wstring_to_utf8(gbk_to_wstring(gbk_str));
    }

    static std::string utf8_to_gbk(const std::string& utf8_str) {
        return wstring_to_gbk(utf8_to_wstring(utf8_str));
    }

    static std::string wstring_to_gbk(const std::wstring& wstr) {
        return wstring_to_multi_byte(wstr, CP_ACP);
    }

    static std::wstring gbk_to_wstring(const std::string& gbk_str) {
        return multi_byte_to_wstring(gbk_str, CP_ACP);
    }

    static std::string wstring_to_utf8(const std::wstring& wstr) {
        return wstring_to_multi_byte(wstr, CP_UTF8);
    }

    static std::wstring utf8_to_wstring(const std::string& utf8_str) {
        return multi_byte_to_wstring(utf8_str, CP_UTF8);
    }
};