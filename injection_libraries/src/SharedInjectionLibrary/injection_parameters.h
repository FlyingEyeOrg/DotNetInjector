#pragma once

#include "shared_memory.h"
#include <string>

class InjectionParameters {
public:
    // 参数配置结构体
    struct Config {
        size_t maxFrameworkVersionLen = 128;   // 最大框架版本长度
        size_t maxAssemblyFileLen = 256;       // 最大程序集路径长度
        size_t maxEntryClassLen = 128;         // 最大入口类名长度
        size_t maxEntryMethodLen = 128;        // 最大入口方法名长度
        size_t maxEntryArgLen = 256;           // 最大参数长度
    };

private:
    Config m_Config;

    /// <summary>
    /// .NET Framework 版本（可选）
    /// 仅在使用 .NET Framework 程序注入时需要
    /// </summary>
    SharedMemory m_FrameworkVersion;
    SharedMemory m_AssemblyFile;
    SharedMemory m_EntryClass;
    SharedMemory m_EntryMethod;
    SharedMemory m_EntryArgument;

    // 内部辅助方法：获取字符串参数
    std::wstring GetParameter(const SharedMemory& shm) const
    {
        if (!shm.IsValid())
            return L"";
        return shm.Read();
    }

public:
    InjectionParameters() = default;

    ~InjectionParameters()
    {
        Close();
    }

    /// <summary>
    /// 连接到已存在的共享内存（作为客户端/使用者）
    /// FrameworkVersion 和 EntryArgument 打开失败会被忽略，因为它们是可选的
    /// </summary>
    bool Open(const Config& config = Config())
    {
        try
        {
            m_Config = config;

            // 可选参数：打开失败忽略
            m_FrameworkVersion.Open(L"FrameworkVersion", sizeof(wchar_t) * m_Config.maxFrameworkVersionLen);
            m_EntryArgument.Open(L"EntryArgument", sizeof(wchar_t) * m_Config.maxEntryArgLen);

            // 必需参数：必须打开成功
            bool success =
                m_AssemblyFile.Open(L"AssemblyFile", sizeof(wchar_t) * m_Config.maxAssemblyFileLen) &&
                m_EntryClass.Open(L"EntryClass", sizeof(wchar_t) * m_Config.maxEntryClassLen) &&
                m_EntryMethod.Open(L"EntryMethod", sizeof(wchar_t) * m_Config.maxEntryMethodLen);

            if (!success)
            {
                Close();
                return false;
            }

            return true;
        }
        catch (...)
        {
            Close();
            return false;
        }
    }

    /// <summary>
    /// 关闭所有共享内存连接
    /// </summary>
    void Close()
    {
        m_FrameworkVersion.Close();
        m_AssemblyFile.Close();
        m_EntryClass.Close();
        m_EntryMethod.Close();
        m_EntryArgument.Close();
    }

    // ============ 参数获取方法 ============

    /// <summary>
    /// 获取 .NET Framework 版本（可选参数）
    /// </summary>
    std::wstring GetFrameworkVersion() const
    {
        return GetParameter(m_FrameworkVersion);
    }

    /// <summary>
    /// 检查是否设置了 Framework 版本
    /// </summary>
    bool HasFrameworkVersion() const
    {
        return !GetFrameworkVersion().empty();
    }

    /// <summary>
    /// 检查是否设置了入口参数
    /// </summary>
    bool HasEntryArgument() const
    {
        return !GetEntryArgument().empty();
    }

    /// <summary>
    /// 获取程序集文件路径（必需）
    /// </summary>
    std::wstring GetAssemblyFile() const
    {
        return GetParameter(m_AssemblyFile);
    }

    /// <summary>
    /// 获取入口类名（必需）
    /// </summary>
    std::wstring GetEntryClass() const
    {
        return GetParameter(m_EntryClass);
    }

    /// <summary>
    /// 获取入口方法名（必需）
    /// </summary>
    std::wstring GetEntryMethod() const
    {
        return GetParameter(m_EntryMethod);
    }

    /// <summary>
    /// 获取入口方法参数（可选）
    /// </summary>
    std::wstring GetEntryArgument() const
    {
        return GetParameter(m_EntryArgument);
    }

    // ============ 状态检查方法 ============

    /// <summary>
    /// 检查所有必需参数是否已设置
    /// </summary>
    bool AreAllRequiredParamsSet() const
    {
        return !GetAssemblyFile().empty() &&
            !GetEntryClass().empty() &&
            !GetEntryMethod().empty();
    }

    /// <summary>
    /// 检查对象是否有效（只检查必需参数）
    /// </summary>
    bool IsValid() const
    {
        return m_AssemblyFile.IsValid() &&
            m_EntryClass.IsValid() &&
            m_EntryMethod.IsValid();
    }

    /// <summary>
    /// 获取所有参数的摘要（用于调试）
    /// </summary>
    std::wstring GetDebugSummary() const
    {
        std::wstring summary;
        summary += L"FrameworkVersion: " + GetFrameworkVersion() + L" (optional)\n";
        summary += L"AssemblyFile: " + GetAssemblyFile() + L" (required)\n";
        summary += L"EntryClass: " + GetEntryClass() + L" (required)\n";
        summary += L"EntryMethod: " + GetEntryMethod() + L" (required)\n";
        summary += L"EntryArgument: " + GetEntryArgument() + L" (optional)\n";
        return summary;
    }
};