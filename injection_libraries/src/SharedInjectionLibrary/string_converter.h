#pragma once

#include <Windows.h>

#include <string>
#include <vector>

/// @brief 字符串转换器
class string_convertor {
public:
    /// @brief 将 GBK 字符串转换成 UTF8 字符串
    /// @param gbk_str 需要转换的 GBK 字符串
    /// @return 返回 UTF8 类型的字符串
    static std::string gbk_to_utf8(const std::string& gbk_str) {
        // Step 1: GBK (CP_ACP) → UTF-16
        int wlen = MultiByteToWideChar(
            CP_ACP,           // 当前 ANSI 代码页（中文 Windows 是 GBK）
            0,                // 标志位
            gbk_str.c_str(),  // 输入 GBK 字符串
            -1,               // 自动计算长度（包括 null 终止符）
            NULL,             // 输出缓冲区（NULL 获取所需大小）
            0                 // 缓冲区大小
        );

        if (wlen <= 0) {
            // 转换失败，返回空字符串或原始字符串
            return "";
        }

        wchar_t* wstr = new wchar_t[wlen];
        MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, wstr, wlen);

        // Step 2: UTF-16 → UTF-8
        int ulen = WideCharToMultiByte(CP_UTF8,  // 目标编码：UTF-8
            0,        // 标志位
            wstr,     // 输入 UTF-16 字符串
            -1,       // 自动计算长度
            NULL,  // 输出缓冲区（NULL 获取所需大小）
            0,     // 缓冲区大小
            NULL,  // 默认字符（转换失败时使用）
            NULL   // 是否使用了默认字符
        );

        if (ulen <= 0) {
            delete[] wstr;
            return "";
        }

        char* utf8_str = new char[ulen];
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_str, ulen, NULL, NULL);

        std::string result(utf8_str);

        // 清理内存
        delete[] wstr;
        delete[] utf8_str;

        return result;
    }

    /// @brief 将 UTF8 字符串转换成 GBK 字符串
    /// @param utf8_str 需要转换的 UTF8 字符串
    /// @return 返回 GBK 类型的字符串
    static std::string utf8_to_gbk(const std::string& utf8_str) {
        int len =
            MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, 0);
        wchar_t* gbk_str = new wchar_t[len];
        MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, gbk_str, len);

        len = WideCharToMultiByte(CP_ACP, 0, gbk_str, -1, NULL, 0, NULL, NULL);
        char* sz_gbk = new char[len];
        WideCharToMultiByte(CP_ACP, 0, gbk_str, -1, sz_gbk, len, NULL, NULL);

        std::string str(sz_gbk);
        delete[] gbk_str;
        delete[] sz_gbk;
        return str;
    }

    /// @brief 将宽字符（UTF-16/Unicode）转 GBK
    /// @param wstr 宽字符字符串
    /// @return 返回 GBK 字符串
    static std::string wstring_to_gbk(const std::wstring& wstr) {
        if (wstr.empty()) return "";

        // 获取需要的缓冲区大小
        int size_needed =
            WideCharToMultiByte(CP_ACP,              // 目标代码页：ANSI（GBK）
                0,                   // 转换选项
                wstr.c_str(),        // 源字符串
                (int)wstr.length(),  // 字符串长度
                NULL,                // 输出缓冲区
                0,                   // 缓冲区大小
                NULL,                // 默认字符
                NULL                 // 是否使用默认字符
            );

        if (size_needed == 0) {
            return "";
        }

        // 分配缓冲区
        std::vector<char> buffer(size_needed + 1);

        // 执行转换
        int result =
            WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(),
                buffer.data(), size_needed, NULL, NULL);

        if (result == 0) {
            return "";
        }

        buffer[result] = '\0';
        return std::string(buffer.data());
    }

    /// @brief 将 GBK 字符串转换成宽字符（UTF-16/Unicode）
    /// @param gbk_str 需要转换的 GBK 字符串
    /// @return 返回宽字符字符串
    static std::wstring gbk_to_wstring(const std::string& gbk_str) {
        if (gbk_str.empty()) return L"";

        // 获取需要的缓冲区大小
        int size_needed =
            MultiByteToWideChar(CP_ACP,                 // 源代码页：ANSI（GBK）
                0,                      // 转换选项
                gbk_str.c_str(),        // 源字符串
                (int)gbk_str.length(),  // 字符串长度
                NULL,                   // 输出缓冲区
                0                       // 缓冲区大小
            );

        if (size_needed == 0) {
            return L"";
        }

        // 分配缓冲区
        std::vector<wchar_t> buffer(size_needed + 1);

        // 执行转换
        int result = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(),
            (int)gbk_str.length(), buffer.data(),
            size_needed);

        if (result == 0) {
            return L"";
        }

        buffer[result] = L'\0';
        return std::wstring(buffer.data());
    }

    /// @brief 将宽字符（UTF-16/Unicode）转换成 UTF8 字符串
    /// @param wstr 宽字符字符串
    /// @return 返回 UTF8 字符串
    static std::string wstring_to_utf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";

        // 获取需要的缓冲区大小
        int size_needed = WideCharToMultiByte(CP_UTF8,  // 目标代码页：UTF-8
            0,        // 转换选项
            wstr.c_str(),        // 源字符串
            (int)wstr.length(),  // 字符串长度
            NULL,                // 输出缓冲区
            0,                   // 缓冲区大小
            NULL,                // 默认字符
            NULL  // 是否使用默认字符
        );

        if (size_needed == 0) {
            return "";
        }

        // 分配缓冲区
        std::vector<char> buffer(size_needed + 1);

        // 执行转换
        int result =
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(),
                buffer.data(), size_needed, NULL, NULL);

        if (result == 0) {
            return "";
        }

        buffer[result] = '\0';
        return std::string(buffer.data());
    }

    /// @brief 将 UTF8 字符串转换成宽字符（UTF-16/Unicode）
    /// @param utf8_str UTF8 字符串
    /// @return 返回宽字符字符串
    static std::wstring utf8_to_wstring(const std::string& utf8_str) {
        if (utf8_str.empty()) return L"";

        // 获取需要的缓冲区大小
        int size_needed =
            MultiByteToWideChar(CP_UTF8,                 // 源代码页：UTF-8
                0,                       // 转换选项
                utf8_str.c_str(),        // 源字符串
                (int)utf8_str.length(),  // 字符串长度
                NULL,                    // 输出缓冲区
                0                        // 缓冲区大小
            );

        if (size_needed == 0) {
            return L"";
        }

        // 分配缓冲区
        std::vector<wchar_t> buffer(size_needed + 1);

        // 执行转换
        int result = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(),
            (int)utf8_str.length(), buffer.data(),
            size_needed);

        if (result == 0) {
            return L"";
        }

        buffer[result] = L'\0';
        return std::wstring(buffer.data());
    }
};