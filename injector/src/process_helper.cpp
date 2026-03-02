#pragma warning(disable: 4819)

#include "process_helper.hpp"

#include <stdio.h>


bool is_correct_target_architecture(HANDLE h_proc) {
    BOOL b_target = FALSE;
    if (!IsWow64Process(h_proc, &b_target)) {
        printf("Can't confirm target process architecture: 0x%X\n",
               GetLastError());
        return false;
    }

    BOOL b_host = FALSE;
    IsWow64Process(GetCurrentProcess(), &b_host);

    return (b_target == b_host);
}

DWORD get_process_id_by_name(const wchar_t* name) {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32First(snapshot, &entry) == TRUE) {
        while (Process32Next(snapshot, &entry) == TRUE) {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                CloseHandle(snapshot);  // thanks to Pvt Comfy
                return entry.th32ProcessID;
            }
        }
    }

    CloseHandle(snapshot);
    return 0;
}