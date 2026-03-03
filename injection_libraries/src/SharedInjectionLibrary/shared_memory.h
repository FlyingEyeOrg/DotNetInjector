#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>

// 定义 BYTE 为 unsigned char，便于表示字节
using BYTE = uint8_t;

class SharedMemory final
{
private:
    HANDLE m_hMapFile = NULL;           // 共享内存句柄
    wchar_t* m_pData = nullptr;         // 映射的内存指针（字符串）
    size_t m_BufferSize = 0;             // 共享内存缓冲区总大小
    size_t m_StringLength = 0;           // 当前存储的字符串长度（含null）

    HANDLE m_hMutex = NULL;             // 命名互斥量，用于跨进程同步
    wchar_t m_MutexName[256];           // 互斥量名称

    // 固定的 GUID，所有进程使用同一个
    static constexpr wchar_t GUID[] = L"a1b2c3d4-e5f6-7890-abcd-ef1234567890";

    // 生成带 GUID 前缀的名称
    static std::wstring MakeUniqueName(const wchar_t* baseName)
    {
        std::wstring uniqueName = L"[";
        uniqueName += GUID;
        uniqueName += L"]-";
        uniqueName += baseName;
        return uniqueName;
    }

public:
    SharedMemory() = default;

    ~SharedMemory()
    {
        Close();
    }

    // 创建共享内存 + 互斥量
    bool Create(const wchar_t* baseName, size_t bufferSize)
    {
        // 生成唯一名称
        std::wstring uniqueName = MakeUniqueName(baseName);

        m_BufferSize = bufferSize;
        m_StringLength = 0;

        // 1. 创建共享内存（使用唯一名称）
        m_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            (DWORD)bufferSize,
            uniqueName.c_str()
        );
        if (!m_hMapFile)
            return false;

        m_pData = static_cast<wchar_t*>(MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, bufferSize));
        if (!m_pData)
        {
            CloseHandle(m_hMapFile);
            m_hMapFile = NULL;
            return false;
        }

        // 清空内存并初始化为空字符串
        memset(m_pData, 0, bufferSize);

        // 2. 创建命名互斥量（使用基础名称）
        swprintf_s(m_MutexName, L"Global\\[%s]-ProcessInjector_SharedMemory_Mutex_%s", GUID, baseName);
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
    bool Open(const wchar_t* baseName, size_t bufferSize)
    {
        // 生成唯一名称（必须与创建时一致）
        std::wstring uniqueName = MakeUniqueName(baseName);

        m_BufferSize = bufferSize;
        m_StringLength = 0;

        // 1. 打开共享内存（使用唯一名称）
        m_hMapFile = OpenFileMappingW(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            uniqueName.c_str()
        );
        if (!m_hMapFile)
            return false;

        m_pData = static_cast<wchar_t*>(MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, bufferSize));
        if (!m_pData)
        {
            CloseHandle(m_hMapFile);
            m_hMapFile = NULL;
            return false;
        }

        // 计算现有字符串长度
        if (m_pData)
        {
            m_StringLength = wcslen(m_pData);
        }

        // 2. 打开同名互斥量（使用基础名称）
        swprintf_s(m_MutexName, L"Global\\ProcessInjector_SharedMemory_Mutex_%s", baseName);
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
        m_BufferSize = 0;
        m_StringLength = 0;
    }

    // 是否有效
    bool IsValid() const
    {
        return m_pData != nullptr && m_hMutex != NULL;
    }

    /// @brief 写入宽字符串到共享内存（跨进程安全）
    /// @param str 要写入的宽字符串
    /// @return 是否写入成功
    bool Write(const std::wstring& str)
    {
        if (!m_pData || !m_hMutex)
            return false;

        size_t strLenWithNull = str.length() + 1;  // 包含 null 终止符
        if (strLenWithNull > m_BufferSize)
            return false;  // 字符串太长，放不下

        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
        if (dwWaitResult != WAIT_OBJECT_0)
            return false;

        // 安全拷贝字符串到共享内存
        wcscpy_s(m_pData, m_BufferSize / sizeof(wchar_t), str.c_str());
        m_StringLength = str.length();

        ReleaseMutex(m_hMutex);
        return true;
    }

    /// @brief 从共享内存读取宽字符串（跨进程安全）
    /// @param outStr 输出字符串
    /// @return 是否读取成功
    bool Read(std::wstring& outStr) const
    {
        if (!m_pData || !m_hMutex)
            return false;

        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
        if (dwWaitResult != WAIT_OBJECT_0)
            return false;

        // 从共享内存复制字符串
        outStr = m_pData;  // wstring 构造函数会读取直到 null 终止符

        ReleaseMutex(m_hMutex);
        return true;
    }

    /// @brief 从共享内存读取宽字符串（返回 wstring）
    /// @return 包含读取数据的字符串（如果失败则返回空字符串）
    std::wstring Read() const
    {
        if (!m_pData || !m_hMutex)
            return L"";

        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
        if (dwWaitResult != WAIT_OBJECT_0)
            return L"";

        std::wstring result = m_pData;

        ReleaseMutex(m_hMutex);
        return result;
    }

    /// @brief 写入 C 风格宽字符串到共享内存
    /// @param str 要写入的 C 风格宽字符串
    /// @return 是否写入成功
    bool Write(const wchar_t* str)
    {
        if (!str)
            return false;
        return Write(std::wstring(str));
    }

    /// @brief 追加字符串到共享内存（如果空间足够）
    /// @param str 要追加的宽字符串
    /// @return 是否追加成功
    bool Append(const std::wstring& str)
    {
        if (!m_pData || !m_hMutex || str.empty())
            return false;

        size_t currentLen = m_StringLength;
        size_t appendLen = str.length();
        size_t newTotalLen = currentLen + appendLen;

        if (newTotalLen + 1 > m_BufferSize)  // +1 为 null 终止符
            return false;  // 空间不足

        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
        if (dwWaitResult != WAIT_OBJECT_0)
            return false;

        // 追加字符串
        wcscat_s(m_pData, m_BufferSize / sizeof(wchar_t), str.c_str());
        m_StringLength = newTotalLen;

        ReleaseMutex(m_hMutex);
        return true;
    }

    // 获取共享内存中字符串的长度（不含 null 终止符）
    size_t GetStringLength() const
    {
        return m_StringLength;
    }

    // 获取共享内存缓冲区总大小（字节）
    size_t GetBufferSize() const
    {
        return m_BufferSize;
    }

    // 检查共享内存是否为空
    bool IsEmpty() const
    {
        return m_StringLength == 0;
    }

    // 清空共享内存中的字符串
    bool Clear()
    {
        if (!m_pData || !m_hMutex)
            return false;

        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, INFINITE);
        if (dwWaitResult != WAIT_OBJECT_0)
            return false;

        m_pData[0] = L'\0';
        m_StringLength = 0;

        ReleaseMutex(m_hMutex);
        return true;
    }

    // 获取共享内存起始地址（只读，用于直接访问）
    const wchar_t* GetData() const
    {
        return m_pData;
    }

    // 获取生成的唯一名称（调试用）
    static std::wstring GetUniqueName(const wchar_t* baseName)
    {
        std::wstring uniqueName = L"[";
        uniqueName += GUID;
        uniqueName += L"]-";
        uniqueName += baseName;
        return uniqueName;
    }
};