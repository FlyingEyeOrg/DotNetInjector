// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <aclapi.h>

// 导出宏
#ifdef SHAREDMEMORYLIB_EXPORTS
#define SHAREDMEMORYLIB_API __declspec(dllexport)
#else
#define SHAREDMEMORYLIB_API __declspec(dllimport)
#endif

// 用 C 链接方式导出
#ifdef __cplusplus
extern "C" {
#endif

	SHAREDMEMORYLIB_API DWORD CreateEmptyReadOnlySharedMemory(
		IN LPCSTR pName,
		IN DWORD dwBufferSize,
		OUT HANDLE* phMapFile);

	SHAREDMEMORYLIB_API DWORD WriteToSharedMemory(
		IN HANDLE hMapFile,
		IN const BYTE* pData,
		IN DWORD dwDataSize);

	SHAREDMEMORYLIB_API DWORD ReleaseSharedMemory(
		IN OUT HANDLE* phMapFile);

	SHAREDMEMORYLIB_API DWORD CreateGlobalMutexWithEveryoneAccess(
		IN LPCSTR pName,
		OUT HANDLE* phMutex);

	SHAREDMEMORYLIB_API DWORD WaitForMutex(
		IN HANDLE hMutex,
		IN DWORD dwMilliseconds);

	SHAREDMEMORYLIB_API DWORD ReleaseMutexHandle(
		IN HANDLE hMutex);

	SHAREDMEMORYLIB_API DWORD CloseMutex(
		IN OUT HANDLE* phMutex);

#ifdef __cplusplus
}
#endif

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

/**
 * 等待互斥量（无限等待或指定超时）
 * 参数：
 *   hMutex         - 互斥量句柄
 *   dwMilliseconds - 等待超时时间（毫秒），INFINITE 表示无限等待
 * 返回：
 *   ERROR_SUCCESS  成功获取互斥量
 *   WAIT_TIMEOUT  超时
 *   WAIT_ABANDONED 前一个持有者异常退出
 *   其他错误码     失败
 */
DWORD WaitForMutex(IN HANDLE hMutex, IN DWORD dwMilliseconds)
{
	if (!hMutex)
	{
		return ERROR_INVALID_PARAMETER;
	}

	DWORD dwWaitResult = WaitForSingleObject(hMutex, dwMilliseconds);

	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		return ERROR_SUCCESS;           // 成功获取互斥量
	case WAIT_ABANDONED:
		return WAIT_ABANDONED;          // 前一个持有者异常退出
	case WAIT_TIMEOUT:
		return WAIT_TIMEOUT;            // 超时
	case WAIT_FAILED:
		return GetLastError();          // 失败
	default:
		return ERROR_INVALID_STATE;     // 未知状态
	}
}

/**
 * 释放互斥量（必须由持有者调用）
 * 参数：
 *   hMutex - 互斥量句柄
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败
 */
DWORD ReleaseMutexHandle(IN HANDLE hMutex)
{
	if (!hMutex)
	{
		return ERROR_INVALID_PARAMETER;
	}

	if (!ReleaseMutex(hMutex))
	{
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

/**
 * 关闭互斥量句柄并置空
 * 参数：
 *   phMutex - [输入输出] 互斥量句柄指针，调用后会被置为 NULL
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败
 */
DWORD CloseMutex(IN OUT HANDLE* phMutex)
{
	DWORD dwResult = ERROR_SUCCESS;

	if (!phMutex)
	{
		return ERROR_INVALID_PARAMETER;
	}

	if (*phMutex)
	{
		if (!CloseHandle(*phMutex))
		{
			dwResult = GetLastError();
		}
		*phMutex = NULL;
	}

	return dwResult;
}

/**
 * 创建带 Everyone 完全访问权限的全局互斥量
 * 参数：
 *   pName        - 互斥量名称（如 "Global\\MyMutex"）
 *   phMutex      - [输出] 返回的互斥量句柄
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败
 */
DWORD CreateGlobalMutexWithEveryoneAccess(IN LPCSTR pName, OUT HANDLE* phMutex)
{
	DWORD dwResult = ERROR_SUCCESS;
	HANDLE hMutex = NULL;
	PSID pEveryoneSID = NULL;
	PACL pACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	EXPLICIT_ACCESS ea = { 0 };
	SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
	SECURITY_ATTRIBUTES sa = { 0 };

	if (phMutex)
		*phMutex = NULL;

	// 1. 为 Everyone 组创建 SID
	if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
		SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0,
		&pEveryoneSID))
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	// 2. 设置 DACL：允许 Everyone 完全访问
	ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
	ea.grfAccessPermissions = MUTEX_ALL_ACCESS;  // 关键：允许所有操作
	ea.grfAccessMode = SET_ACCESS;
	ea.grfInheritance = NO_INHERITANCE;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;

	if (SetEntriesInAcl(1, &ea, NULL, &pACL) != ERROR_SUCCESS)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	// 3. 创建安全描述符
	pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (!pSD)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}
	if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
	{
		dwResult = GetLastError();
		goto Cleanup;
	}
	if (!SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE))
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	// 4. 设置安全属性
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = FALSE;

	// 5. 创建全局互斥量
	hMutex = CreateMutexA(
		&sa,
		FALSE,  // 初始不拥有
		pName);

	if (hMutex == NULL)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	if (phMutex)
		*phMutex = hMutex;

Cleanup:
	if (pEveryoneSID) FreeSid(pEveryoneSID);
	if (pACL) LocalFree(pACL);
	if (pSD) LocalFree(pSD);

	if (dwResult != ERROR_SUCCESS && phMutex)
		*phMutex = NULL;

	return dwResult;
}

/**
 * 释放共享内存句柄及相关资源
 * 参数：
 *   phMapFile - [输入输出] 共享内存句柄指针，调用后会被置为 NULL
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败（可通过 FormatMessage 获取描述）
 */
DWORD ReleaseSharedMemory(IN OUT HANDLE* phMapFile)
{
	DWORD dwResult = ERROR_SUCCESS;

	if (!phMapFile)
	{
		return ERROR_INVALID_PARAMETER;
	}

	if (*phMapFile)
	{
		if (!CloseHandle(*phMapFile))
		{
			dwResult = GetLastError();
		}
		*phMapFile = NULL;
	}

	return dwResult;
}

/**
 * 向已存在的共享内存写入二进制数据（字节流）
 * 参数：
 *   hMapFile     - 共享内存句柄（由 CreateEmptyReadOnlySharedMemory 返回）
 *   pData        - 要写入的字节流指针（const BYTE*）
 *   dwDataSize   - 要写入的字节数，不能超过共享内存大小
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败（可通过 FormatMessage 获取描述）
 */
DWORD WriteToSharedMemory(IN HANDLE hMapFile, IN const BYTE* pData, IN DWORD dwDataSize)
{
	DWORD dwResult = ERROR_SUCCESS;
	LPVOID pView = NULL;

	if (!hMapFile || !pData || dwDataSize == 0)
	{
		return ERROR_INVALID_PARAMETER;
	}

	pView = MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, dwDataSize);
	if (!pView)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	CopyMemory(pView, pData, dwDataSize);

Cleanup:
	if (pView)
		UnmapViewOfFile(pView);

	return dwResult;
}


/**
 * 创建带 Everyone 只读权限的共享内存
 * 参数：
 *   pName        - 共享内存名称（如 "Global\\MyFileMappingObject"）
 *   dwBufferSize - 共享内存大小（字节）
 *   phMapFile    - [输出] 返回的共享内存句柄，失败时为 NULL
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败（可通过 FormatMessage 获取描述）
 */
DWORD CreateEmptyReadOnlySharedMemory(IN LPCSTR pName, IN DWORD dwBufferSize, OUT HANDLE* phMapFile)
{
	DWORD dwResult = ERROR_SUCCESS;
	HANDLE hMapFile = NULL;
	PSID pEveryoneSID = NULL;
	PACL pACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	EXPLICIT_ACCESS ea = { 0 };
	SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
	SECURITY_ATTRIBUTES sa = { 0 };

	if (phMapFile)
		*phMapFile = NULL;

	if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
		SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0,
		&pEveryoneSID))
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
	ea.grfAccessPermissions = FILE_MAP_READ;
	ea.grfAccessMode = SET_ACCESS;
	ea.grfInheritance = NO_INHERITANCE;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;

	if (SetEntriesInAcl(1, &ea, NULL, &pACL) != ERROR_SUCCESS)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (!pSD)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}
	if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	if (!SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE))
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = FALSE;

	hMapFile = CreateFileMappingA(
		INVALID_HANDLE_VALUE,
		&sa,
		PAGE_READWRITE,
		0,
		dwBufferSize,
		pName);

	if (hMapFile == NULL)
	{
		dwResult = GetLastError();
		goto Cleanup;
	}

	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		LPVOID pView = MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, dwBufferSize);
		if (pView)
		{
			ZeroMemory(pView, dwBufferSize);
			UnmapViewOfFile(pView);
		}
	}

	if (phMapFile)
		*phMapFile = hMapFile;

Cleanup:
	if (pEveryoneSID) FreeSid(pEveryoneSID);
	if (pACL) LocalFree(pACL);
	if (pSD) LocalFree(pSD);

	if (dwResult != ERROR_SUCCESS && phMapFile)
		*phMapFile = NULL;

	return dwResult;
}
