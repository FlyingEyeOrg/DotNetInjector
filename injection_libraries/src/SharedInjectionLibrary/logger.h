#pragma once

#include <string>
#include <mutex>
#include <chrono>
#include <filesystem>
#include "pch.h"

class Logger final
{
private:
    static constexpr const wchar_t* LogID = APP_ID;

    static std::wstring s_LogDir;
    static std::wstring s_LogFile;
    static std::mutex s_LockObj;

    static std::wstring GetBaseDirectory();
    static void InternalInitialize();
    static std::string ToUtf8(const std::wstring& wstr);
    static std::string GetTimestamp();

public:
    Logger() = delete;
    ~Logger() = delete;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void Initialize();
    static void Log(const std::string& message);

    template<typename... Args>
    static void Log(const std::string& format, Args... args)
    {
        char buffer[4096];
        snprintf(buffer, sizeof(buffer), format.c_str(), args...);
        Log(std::string(buffer));
    }
};