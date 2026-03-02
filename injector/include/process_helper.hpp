#pragma once

/* include windows header */
#include <Windows.h>
/* include TlHelp32 header*/
#include <TlHelp32.h>

// 判断进程是否符合正确的 x86/x64 架构
bool is_correct_target_architecture(HANDLE h_proc);

// 根据进程名称获取进程 id
DWORD get_process_id_by_name(wchar_t* name);