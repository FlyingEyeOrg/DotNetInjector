#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

#include <windows.h>
#include <stdio.h>
#include <aclapi.h>
#include <tchar.h>
#include <iostream>

#define BUF_SIZE 256
CHAR szName[] = "Global\\[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-MyFileMappingObject";
TCHAR  wszName[] = TEXT("Global\\[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-MyFileMappingObject");

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
 * 打开已存在的共享内存（常规方式，只读访问）
 * 参数：
 *   pName        - 共享内存名称（如 "Global\\MyFileMappingObject"）
 *   phMapFile    - [输出] 返回的共享内存句柄，失败时为 NULL
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败（可通过 FormatMessage 获取描述）
 */
DWORD OpenReadOnlySharedMemory(IN LPCSTR pName, OUT HANDLE* phMapFile)
{
	DWORD dwResult = ERROR_SUCCESS;
	HANDLE hMapFile = NULL;

	// 初始化输出参数为 NULL
	if (phMapFile)
		*phMapFile = NULL;

	// 参数校验
	if (!pName || !phMapFile)
	{
		return ERROR_INVALID_PARAMETER;
	}

	// 打开共享内存（只读访问）
	hMapFile = OpenFileMappingA(
		FILE_MAP_READ,      // 只读访问
		FALSE,              // 不继承句柄
		pName);             // 名称

	if (hMapFile == NULL)
	{
		dwResult = GetLastError();
	}
	else
	{
		*phMapFile = hMapFile;
	}

	return dwResult;
}

/**
 * 打开已存在的共享内存（常规方式，只读访问）
 * 参数：
 *   pName        - 共享内存名称（如 "Global\\MyFileMappingObject"）
 *   phMapFile    - [输出] 返回的共享内存句柄，失败时为 NULL
 * 返回：
 *   ERROR_SUCCESS  成功
 *   其他错误码     失败（可通过 FormatMessage 获取描述）
 */
DWORD OpenReadOnlySharedMemoryW(IN LPCWSTR pName, OUT HANDLE* phMapFile)
{
	DWORD dwResult = ERROR_SUCCESS;
	HANDLE hMapFile = NULL;

	// 初始化输出参数为 NULL
	if (phMapFile)
		*phMapFile = NULL;

	// 参数校验
	if (!pName || !phMapFile)
	{
		return ERROR_INVALID_PARAMETER;
	}

	// 打开共享内存（只读访问）
	hMapFile = OpenFileMappingW(
		FILE_MAP_READ,      // 只读访问
		FALSE,              // 不继承句柄
		pName);             // 名称

	if (hMapFile == NULL)
	{
		dwResult = GetLastError();
	}
	else
	{
		*phMapFile = hMapFile;
	}

	return dwResult;
}

void main()
{
	std::cout << "running..." << std::endl;
	std::cout << "name: " << szName << std::endl;

	HANDLE hMapFile = NULL;
	TCHAR* pReadBuf = NULL;

	DWORD dwErr = CreateEmptyReadOnlySharedMemory(szName, BUF_SIZE, &hMapFile);
	if (dwErr == ERROR_SUCCESS)
	{
		// 宽字符串（Unicode）
		wchar_t writeData[] = L"Hello, 世界!";

		// 计算字节数：字符数 × sizeof(wchar_t)
		DWORD dataSizeBytes = (DWORD)(wcslen(writeData) + 1) * sizeof(wchar_t);

		// 写入共享内存
		WriteToSharedMemory(hMapFile, (const BYTE*)writeData, dataSizeBytes);

		// 验证创建者能读到
		pReadBuf = (TCHAR*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, BUF_SIZE);
		if (pReadBuf)
		{
			// 比较宽字符串内容
			bool same = (memcmp(pReadBuf, writeData, dataSizeBytes) == 0);
			// same == true 说明数据一致
			UnmapViewOfFile(pReadBuf);
		}
	}

	HANDLE hOpenMapFile = NULL;
	dwErr = OpenReadOnlySharedMemoryW(wszName, &hOpenMapFile);

	if (dwErr == ERROR_SUCCESS)
	{
		// 验证打开者能读到相同数据
		pReadBuf = (TCHAR*)MapViewOfFile(hOpenMapFile, FILE_MAP_READ, 0, 0, BUF_SIZE);
		
		ReleaseSharedMemory(&hOpenMapFile);
	}

	getchar();

	if (hMapFile)
		ReleaseSharedMemory(&hMapFile);
}