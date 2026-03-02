#pragma warning(disable : 4819)
#include <stdio.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "injector.hpp"
#include "process_helper.hpp"
#include "resource.hpp"
#include "string_convertor.hpp"

using namespace std;

// 解析 PID 字符串，返回是否成功
bool parse_pid(const std::string& pid_str, DWORD& out_pid) {
    // 空字符串检查
    if (pid_str.empty()) {
        std::cerr << "[ERROR] PID cannot be empty" << std::endl;
        return false;
    }

    // 检查每个字符是否是数字
    for (char c : pid_str) {
        if (c < '0' || c > '9') {
            std::cerr << "[ERROR] Invalid PID format: \"" << pid_str
                      << "\" (must be numeric)" << std::endl;
            return false;
        }
    }

    // 转换为数字
    try {
        out_pid = static_cast<DWORD>(std::stoul(pid_str));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to convert PID: " << e.what() << std::endl;
        return false;
    }
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    // ==================== 1. GBK 编码转换 ====================
    std::vector<char*> gbk_argv(argc);

    for (int i = 0; i < argc; ++i) {
        std::string gbk_str = string_convertor::wstring_to_gbk(argv[i]);
        gbk_argv[i] = new char[gbk_str.size() + 1];  // 分配内存
        strcpy(gbk_argv[i], gbk_str.c_str());        // 拷贝内容
        std::cout << "[DEBUG] arg" << i << ": \"" << gbk_argv[i] << "\""
                  << std::endl;
    }

    // ==================== 2. 定义命令行选项 ====================
    cxxopts::Options options(
        "injector", "DLL Injector - Inject a DLL into a running process");

    options.add_options()("h,help", "Show this help message and exit")(
        "n,name", "Target process name (e.g., notepad.exe)",
        cxxopts::value<std::string>())(
        "p,pid", "Target process ID (e.g., 1234)", cxxopts::value<string>())(
        "dllpath", "The DLL path to inject (required positional)",
        cxxopts::value<std::string>());

    // 只定义 dllpath 为位置参数
    options.positional_help("<dllpath>");
    options.parse_positional({"dllpath"});

    // 允许未识别的选项
    options.allow_unrecognised_options();

    // ==================== 3. 解析命令行 ====================
    cxxopts::ParseResult result;
    result = options.parse(argc, gbk_argv.data());

    // ==================== 4. 处理帮助请求 ====================
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    // ==================== 5. 验证必需参数 ====================
    if (!result.count("dllpath")) {
        std::cerr << "[ERROR] Missing required argument: <dllpath>"
                  << std::endl;
        std::cerr << "Usage: " << options.help() << std::endl;
        return 1;
    }

    // ==================== 6. 提取参数 ====================
    std::string gbk_dllpath = result["dllpath"].as<std::string>();
    std::string process_name;
    DWORD process_id = 0;

    // 检查各种输入方式
    bool by_name_opt = result.count("name") > 0;  // -n 或 --name
    bool by_pid_opt = result.count("pid") > 0;    // -p 或 --pid

    // 从选项获取进程名
    if (by_name_opt) {
        process_name = result["name"].as<std::string>();
    }

    // 从选项获取进程ID
    if (by_pid_opt) {
        std::string process_id_str = result["pid"].as<std::string>();
        if (!parse_pid(process_id_str, process_id)) {
            return 1;  // 解析失败
        }
    }

    // ==================== 7. 互斥性检查 ====================
    int specified_count = (by_name_opt ? 1 : 0) + (by_pid_opt ? 1 : 0);

    if (specified_count == 0) {
        std::cerr << "[ERROR] Must specify target process using one of:"
                  << std::endl;
        std::cerr << "  -n <name> or --name <name>  (process name)"
                  << std::endl;
        std::cerr << "  -p <pid> or --pid <pid>      (process ID)" << std::endl;
        return 1;
    }

    if (specified_count > 1) {
        std::cerr
            << "[ERROR] Cannot specify multiple target process identifiers:"
            << std::endl;
        std::cerr << "  -n/--name and -p/--pid are mutually exclusive"
                  << std::endl;
        return 1;
    }

    // ==================== 8. 打印配置摘要 ====================
    std::cout << "\n========== Injection Configuration ==========" << std::endl;
    std::cout << "DLL Path:      " << gbk_dllpath << std::endl;

    if (by_pid_opt) {
        std::cout << "Target:        PID " << process_id << std::endl;
    } else {
        std::cout << "Target:        Process \"" << process_name << "\""
                  << std::endl;
    }

    std::cout << "Method:        "
              << (by_pid_opt ? "By Process ID" : "By Name (option)")
              << std::endl;
    std::cout << "==============================================\n"
              << std::endl;

    // 对接上面从命令行中解析出来的参数
    auto w_dllpath = string_convertor::gbk_to_wstring(gbk_dllpath);
    auto w_processname = string_convertor::gbk_to_wstring(process_name);

    if (by_name_opt) {
        process_id = get_process_id_by_name(w_processname.c_str());
    }

    if (process_id == 0) {
        printf("Process not found\n");
        system("pause");
        return -1;
    }

    printf("Process pid: %d\n", process_id);

    TOKEN_PRIVILEGES priv = {0};
    HANDLE h_token = NULL;
    if (OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &h_token)) {
        priv.PrivilegeCount = 1;
        priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
            AdjustTokenPrivileges(h_token, FALSE, &priv, 0, NULL, NULL);

        CloseHandle(h_token);
    }

    HANDLE h_proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
    if (!h_proc) {
        DWORD err = GetLastError();
        printf("OpenProcess failed: 0x%X\n", err);
        system("PAUSE");
        return -2;
    }

    if (!is_correct_target_architecture(h_proc)) {
        printf("Invalid Process Architecture.\n");
        CloseHandle(h_proc);
        system("PAUSE");
        return -3;
    }

    if (GetFileAttributes(w_dllpath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("Dll file doesn't exist\n");
        CloseHandle(h_proc);
        system("PAUSE");
        return -4;
    }

    std::ifstream file(w_dllpath, std::ios::binary | std::ios::ate);

    if (file.fail()) {
        printf("Opening the file failed: %X\n", (DWORD)file.rdstate());
        file.close();
        CloseHandle(h_proc);
        system("PAUSE");
        return -5;
    }

    auto file_size = file.tellg();
    if (file_size < 0x1000) {
        printf("Filesize invalid.\n");
        file.close();
        CloseHandle(h_proc);
        system("PAUSE");
        return -6;
    }

    BYTE* p_src_data = new BYTE[(UINT_PTR)file_size];
    if (!p_src_data) {
        printf("Can't allocate dll file.\n");
        file.close();
        CloseHandle(h_proc);
        system("PAUSE");
        return -7;
    }

    file.seekg(0, std::ios::beg);
    file.read((char*)(p_src_data), file_size);
    file.close();

    printf("Mapping...\n");
    if (!manual_map_dll(h_proc, p_src_data, file_size)) {
        delete[] p_src_data;
        CloseHandle(h_proc);
        printf("Error while mapping.\n");
        system("PAUSE");
        return -8;
    }

    delete[] p_src_data;
    CloseHandle(h_proc);

    // 释放 gbk_argv 分配的内存
    for (int i = 0; i < argc; ++i) {
        delete[] gbk_argv[i];
    }

    printf("OK\n");
    return 0;
}