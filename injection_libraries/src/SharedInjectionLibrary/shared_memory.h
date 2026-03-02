#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>  // for BYTE
#include <cstring>  // for memcpy

// 定义 BYTE 为 unsigned char，便于表示字节
using BYTE = uint8_t;

class SharedMemory final
{
private:
	HANDLE m_hMapFile = NULL;           // 共享内存句柄
	void* m_pData = nullptr;            // 映射的内存指针
	size_t m_Size = 0;                  // 共享内存总大小

	HANDLE m_hMutex = NULL;             // 命名互斥量，用于跨进程同步
	wchar_t m_MutexName[256];           // 互斥量名称

public:
	SharedMemory() = default;

	~SharedMemory()
	{
		Close();
	}

	// 创建共享内存 + 互斥量
	bool Create(const wchar_t* name, size_t size)
	{
		m_Size = size;

		// 1. 创建共享内存
		m_hMapFile = CreateFileMappingW(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			(DWORD)size,
			name
		);
		if (!m_hMapFile)
			return false;

		m_pData = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
		if (!m_pData)
		{
			CloseHandle(m_hMapFile);
			m_hMapFile = NULL;
			return false;
		}

		// 可选：清空内存
		memset(m_pData, 0, size);

		// 2. 创建命名互斥量
		swprintf_s(m_MutexName, L"Global\\ProcessInjector_SharedMemory_Mutex_%s", name);
		m_hMutex = CreateMutexW(
			NULL,
			FALSE,
			m_MutexName
		);

		if (!m_hMutex)
		{
			Close();
			return false;
		}

		return true;
	}

	// 打开已有的共享内存 + 互斥量
	bool Open(const wchar_t* name, size_t size)
	{
		m_Size = size;

		// 1. 打开共享内存
		m_hMapFile = OpenFileMappingW(
			FILE_MAP_ALL_ACCESS,
			FALSE,
			name
		);
		if (!m_hMapFile)
			return false;

		m_pData = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
		if (!m_pData)
		{
			CloseHandle(m_hMapFile);
			m_hMapFile = NULL;
			return false;
		}

		// 2. 打开同名互斥量
		swprintf_s(m_MutexName, L"Global\\ProcessInjector_SharedMemory_Mutex_%s", name);
		m_hMutex = OpenMutexW(
			MUTEX_ALL_ACCESS,
			FALSE,
			m_MutexName
		);

		if (!m_hMutex)
		{
			Close();
			return false;
		}

		return true;
	}

	// 关闭所有资源
	void Close()
	{
		if (m_pData)
		{
			UnmapViewOfFile(m_pData);
			m_pData = nullptr;
		}
		if (m_hMapFile)
		{
			CloseHandle(m_hMapFile);
			m_hMapFile = NULL;
		}
		if (m_hMutex)
		{
			CloseHandle(m_hMutex);
			m_hMutex = NULL;
		}
	}

	// 是否有效
	bool IsValid() const
	{
		return m_pData != nullptr && m_hMutex != NULL;
	}

	/// @brief 写入字节流数据到共享内存（跨进程安全）
	/// @param data 指向要写入的数据缓冲区
	/// @param dataSize 数据的字节数
	/// @return 是否写入成功
	bool Write(const void* data, size_t dataSize)
	{
		if (!m_pData || !data || !m_hMutex || dataSize > m_Size)
			return false;

		DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
		if (dwWaitResult != WAIT_OBJECT_0)
			return false;

		// 安全拷贝数据到共享内存
		memcpy(m_pData, data, dataSize);

		ReleaseMutex(m_hMutex);
		return true;
	}

	/// @brief 从共享内存读取字节流数据（跨进程安全）
	/// @param outBuffer 用户提供的输出缓冲区
	/// @param bufferSize 用户缓冲区大小
	/// @param bytesRead [out] 实际读取的字节数
	/// @return 是否读取成功
	bool Read(void* outBuffer, size_t bufferSize, size_t* bytesRead) const
	{
		if (!m_pData || !outBuffer || !bytesRead || !m_hMutex || bufferSize == 0)
			return false;

		DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
		if (dwWaitResult != WAIT_OBJECT_0)
			return false;

		// 确定实际能拷贝多少字节
		size_t safeCopySize = (bufferSize < m_Size) ? bufferSize : m_Size;
		memcpy(outBuffer, m_pData, safeCopySize);
		*bytesRead = safeCopySize;

		ReleaseMutex(m_hMutex);
		return true;
	}

	/// @brief 从共享内存读取全部字节流数据（返回 vector<BYTE>）
	/// @return 包含读取数据的字节流（如果失败则返回空 vector）
	std::vector<BYTE> Read() const
	{
		if (!m_pData || !m_hMutex)
			return {};

		DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
		if (dwWaitResult != WAIT_OBJECT_0)
			return {};

		// 读取全部共享内存内容
		std::vector<BYTE> result(m_Size);
		memcpy(result.data(), m_pData, m_Size);

		ReleaseMutex(m_hMutex);
		return result;
	}

	// 获取共享内存起始地址（只读）
	const void* GetData() const
	{
		return m_pData;
	}
};