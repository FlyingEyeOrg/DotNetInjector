// Logger.h
#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

using std::string;
using std::mutex;

class Logger {
private:
    // 唯一标识符，你可以按需修改
    static constexpr const char* LibraryID = "1665a62f-1f65-4857-b720-6f9216ae5054";

    // 日志文件完整路径
    static std::string LogFile;

    // 用于线程同步的锁
    static std::mutex _lockObj;

    // 私有构造，防止实例化
    Logger() = delete;

    // 初始化日志目录和文件路径（静态构造逻辑）
    static void Initialize();

public:
    // 日志记录接口
    static void Log(const std::string& message);

    // 格式化日志（类似 string.Format）
    static void Log(const char* format, ...);
};

