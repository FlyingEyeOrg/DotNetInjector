#pragma once

#include <windows.h>
#include <string>

class SharedMemory final
{
private:
    HANDLE m_hMapFile = NULL;
    void* m_pData = nullptr;
    size_t m_Size = 0;
    SRWLOCK m_Lock;  // ¶ÁÐ´Ëø

public:
    SharedMemory() = default;

    ~SharedMemory()
    {
        Close();
    }

    bool Create(const wchar_t* name, size_t size)
    {
        m_Size = size;
        m_hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, name);
        if (!m_hMapFile) return false;

        m_pData = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (m_pData) memset(m_pData, 0, size);
        return m_pData != nullptr;
    }

    bool Open(const wchar_t* name, size_t size)
    {
        m_Size = size;
        m_hMapFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (!m_hMapFile) return false;

        m_pData = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
        return m_pData != nullptr;
    }

    void Close()
    {
        if (m_pData) { UnmapViewOfFile(m_pData); m_pData = nullptr; }
        if (m_hMapFile) { CloseHandle(m_hMapFile); m_hMapFile = NULL; }
    }

    bool IsValid() const
    {
        return m_pData != nullptr;
    }

    // Ð´£¨¶ÀÕ¼Ëø£©
    bool Write(const char* str)
    {
        if (!m_pData || !str) return false;

        AcquireSRWLockExclusive(&m_Lock);
        strcpy_s((char*)m_pData, m_Size, str);
        ReleaseSRWLockExclusive(&m_Lock);

        return true;
    }

    // ¶Á£¨¹²ÏíËø£©
    std::string Read() const
    {
        if (!m_pData) return "";

        //PSRWLOCK
        //PSRWLOCK
        SRWLOCK* kk = &m_Lock;
        AcquireSRWLockShared(&m_Lock);
        std::string result((char*)m_pData);
        ReleaseSRWLockShared(&m_Lock);

        return result;
    }
};