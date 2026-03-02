#include <stdio.h>

#include <iostream>
#include <string>

#include "injector.hpp"
#include "resource.hpp"
#include "process_helper.hpp"

using namespace std;

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    const wchar_t* dll_path = nullptr;
    DWORD pid = 0;
    if (argc == 3) {
        dll_path = argv[1];
        pid = get_process_id_by_name(argv[2]);
    } else if (argc == 2) {
        dll_path = argv[1];
        std::string pname;
        printf("Process Name:\n");
        std::getline(std::cin, pname);

        char* v_in = (char*)pname.c_str();
        wchar_t* v_out = new wchar_t[strlen(v_in) + 1];
        mbstowcs_s(NULL, v_out, strlen(v_in) + 1, v_in, strlen(v_in));
        pid = get_process_id_by_name(v_out);
    } else {
        printf("Invalid Params\n");
        printf("Usage: dll_path [process_name]\n");
        system("pause");
        return 0;
    }

    if (pid == 0) {
        printf("Process not found\n");
        system("pause");
        return -1;
    }

    printf("Process pid: %d\n", pid);

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

    HANDLE h_proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
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

    if (GetFileAttributes(dll_path) == INVALID_FILE_ATTRIBUTES) {
        printf("Dll file doesn't exist\n");
        CloseHandle(h_proc);
        system("PAUSE");
        return -4;
    }

    std::ifstream file(dll_path, std::ios::binary | std::ios::ate);

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
    printf("OK\n");
    return 0;
}