#include "pch.h"

#include "logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

std::wstring Logger::log_dir_;
std::wstring Logger::log_file_;
std::mutex Logger::lock_;
std::once_flag Logger::init_flag_;

std::wstring Logger::get_temp_directory() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = ::GetTempPathW(MAX_PATH, buffer);
    if (length > 0 && length < MAX_PATH) {
        std::wstring temp_path(buffer);
        if (!temp_path.empty() && (temp_path.back() == L'\\' || temp_path.back() == L'/')) {
            temp_path.pop_back();
        }
        return temp_path;
    }

    return L".";
}

void Logger::internal_initialize() {
    log_dir_ = get_temp_directory() + L"\\[" + k_log_id + L"]-InjectionLogs";
    std::filesystem::create_directories(log_dir_);
    log_file_ = log_dir_ + L"\\injection.log";
}

std::string Logger::get_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm local_time{};
    localtime_s(&local_time, &time);

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << milliseconds.count();
    return stream.str();
}

void Logger::initialize() {
    std::call_once(init_flag_, []() {
        internal_initialize();
    });
}

void Logger::log(const std::string& message) {
    initialize();

    const std::string log_entry = get_timestamp() + " - " + message + "\r\n";
    std::lock_guard<std::mutex> lock(lock_);

    std::ofstream output_stream(std::filesystem::path(log_file_), std::ios::app | std::ios::binary);
    output_stream.write(log_entry.c_str(), static_cast<std::streamsize>(log_entry.length()));
}