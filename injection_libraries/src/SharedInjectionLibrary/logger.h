#pragma once

#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>

#include "framework.h"

class Logger final {
private:
    static constexpr const wchar_t* k_log_id = APP_ID;

    static std::wstring log_dir_;
    static std::wstring log_file_;
    static std::mutex lock_;
    static std::once_flag init_flag_;

    static std::wstring get_temp_directory();
    static void internal_initialize();
    static std::string get_timestamp();

public:
    Logger() = delete;
    ~Logger() = delete;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void initialize();
    static void log(const std::string& message);

    template<typename... Args>
    static void log(const std::string& format, Args... args) {
        char buffer[4096];
        std::snprintf(buffer, sizeof(buffer), format.c_str(), args...);
        log(std::string(buffer));
    }
};