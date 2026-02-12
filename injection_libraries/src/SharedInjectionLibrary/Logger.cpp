#include "pch.h"
#include "Logger.h"
#include <cstdarg>
#include <cstdio>

// 静态成员定义
std::string Logger::LogFile;
std::mutex Logger::_lockObj;

// 初始化日志目录与文件路径
void Logger::Initialize() {
	static bool initialized = false;
	if (initialized) return;
	initialized = true;

	try {
		// 获取当前模块所在目录（类似于 AppDomain.CurrentDomain.BaseDirectory）
		// 注意：这里使用当前工作目录，如需模块目录，Windows 可用 GetModuleFileName
		std::string baseDir = std::filesystem::current_path().string();

		// 构造日志目录名：InjectionLogs[{LibraryID}]
		std::string logDir = baseDir + "\\InjectionLogs[" + LibraryID + "]";

		// 创建目录（如果不存在）
		std::filesystem::create_directories(logDir);

		// 日志文件路径
		LogFile = logDir + "\\injection.log";
	}
	catch (...) {
		// 目录初始化失败也没关系，后面 FileAppend 会失败但被 catch
	}
}

// 日志记录（普通字符串）
void Logger::Log(const std::string& message) {
	try {
		// 加锁
		std::lock_guard<std::mutex> lock(_lockObj);

		// 获取当前时间
		auto now = std::chrono::system_clock::now();
		auto now_time_t = std::chrono::system_clock::to_time_t(now);
		struct tm now_tm;
		errno_t err = localtime_s(&now_tm, &now_time_t);
		if (err != 0) {
			// 时间转换失败，跳过或记录错误
			return;
		}

		// 格式化为：yyyy-MM-dd HH:mm:ss
		std::ostringstream timeStream;
		timeStream << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");

		std::string logEntry = timeStream.str() + " - " + message + "\n";

		// 写入文件
		if (!LogFile.empty()) {
			std::ofstream ofs(LogFile, std::ios::app); // 追加模式
			if (ofs.is_open()) {
				ofs << logEntry;
			}
		}
	}
	catch (...) {
		// 忽略所有异常
	}
}

// 格式化日志（类似 string.Format）
void Logger::Log(const char* format, ...) {
	try {
		char buffer[4096]; // 适当大小的缓冲区，可根据需要调整

		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);

		// 调用上面的 Log(string)
		Log(std::string(buffer));
	}
	catch (...) {
		// 忽略所有异常
	}
}

