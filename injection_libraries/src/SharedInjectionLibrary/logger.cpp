#include "pch.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <windows.h>

std::wstring Logger::s_LogDir;
std::wstring Logger::s_LogFile;
std::mutex Logger::s_LockObj;

std::wstring Logger::GetTempDirectory()
{
	wchar_t buffer[MAX_PATH];
	DWORD len = GetTempPathW(MAX_PATH, buffer);
	if (len > 0 && len < MAX_PATH)
	{
		std::wstring tempPath(buffer);
		// 移除末尾的反斜杠（如果有）
		if (!tempPath.empty() && (tempPath.back() == L'\\' || tempPath.back() == L'/'))
		{
			tempPath.pop_back();
		}
		return tempPath;
	}
	// 如果获取失败，返回当前目录作为后备
	return L".";
}

void Logger::InternalInitialize()
{
	s_LogDir = GetTempDirectory() + L"\\" + L"[" + LogID + L"]-InjectionLogs";
	std::filesystem::create_directories(s_LogDir);
	s_LogFile = s_LogDir + L"\\injection.log";
}

std::string Logger::ToUtf8(const std::wstring& wstr)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
	return result;
}

std::string Logger::GetTimestamp()
{
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::tm tm;
	localtime_s(&tm, &time);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
		<< '.' << std::setfill('0') << std::setw(3) << ms.count();
	return oss.str();
}

void Logger::Initialize()
{
	static bool initialized = false;
	if (!initialized)
	{
		InternalInitialize();
		initialized = true;
	}
}

void Logger::Log(const std::string& message)
{
	std::string logEntry = GetTimestamp() + " - " + message + "\r\n";

	std::lock_guard<std::mutex> lock(s_LockObj);

	std::ofstream ofs(ToUtf8(s_LogFile), std::ios::app | std::ios::binary);
	ofs.write(logEntry.c_str(), static_cast<std::streamsize>(logEntry.length()));
}