#include "managed_runtime_resolver.h"

#include <Windows.h>

#include <algorithm>
#include <cwctype>

namespace {

std::wstring trim_copy(const std::wstring& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(),
                                        [](wchar_t ch) { return std::iswspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(),
                                      [](wchar_t ch) { return std::iswspace(ch) != 0; })
                         .base();

    if (begin >= end) {
        return L"";
    }

    return std::wstring(begin, end);
}

std::wstring to_lower_copy(const std::wstring& value) {
    std::wstring lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return lower;
}

bool contains_token(const std::wstring& value, const wchar_t* token) {
    return value.find(token) != std::wstring::npos;
}

}  // namespace

ManagedRuntimeKind parse_runtime_hint(const std::wstring& framework_version_hint) {
    const auto trimmed_value = trim_copy(framework_version_hint);
    if (trimmed_value.empty()) {
        return ManagedRuntimeKind::unknown;
    }

    const auto normalized_value = to_lower_copy(trimmed_value);

    if (contains_token(normalized_value, L"mono")) {
        return ManagedRuntimeKind::mono;
    }

    if (contains_token(normalized_value, L"netcore") ||
        contains_token(normalized_value, L"dotnet") ||
        contains_token(normalized_value, L".net")) {
        if (normalized_value.rfind(L"v", 0) != 0) {
            return ManagedRuntimeKind::dotnet;
        }
    }

    if (normalized_value.rfind(L"v", 0) == 0) {
        return ManagedRuntimeKind::dotnet_framework;
    }

    return ManagedRuntimeKind::unknown;
}

ManagedRuntimeKind resolve_runtime_kind(const std::wstring& framework_version_hint,
                                        const RuntimePresence& presence) {
    const auto hinted_runtime = parse_runtime_hint(framework_version_hint);
    if (hinted_runtime != ManagedRuntimeKind::unknown) {
        return hinted_runtime;
    }

    if (presence.has_mono) {
        return ManagedRuntimeKind::mono;
    }

    if (presence.has_coreclr) {
        return ManagedRuntimeKind::dotnet;
    }

    if (presence.has_mscoree) {
        return ManagedRuntimeKind::dotnet_framework;
    }

    return ManagedRuntimeKind::unknown;
}

RuntimePresence detect_runtime_presence() {
    RuntimePresence presence{};
    presence.has_coreclr = ::GetModuleHandleW(L"coreclr.dll") != nullptr ||
                           ::GetModuleHandleW(L"coreclr.so") != nullptr ||
                           ::GetModuleHandleW(L"coreclr.dynlib") != nullptr;
    presence.has_mscoree = ::GetModuleHandleW(L"mscoree.dll") != nullptr;
    presence.has_mono = ::GetModuleHandleW(L"mono-2.0-sgen.dll") != nullptr ||
                        ::GetModuleHandleW(L"mono-2.0-bdwgc.dll") != nullptr;
    return presence;
}

const char* to_string(ManagedRuntimeKind runtime_kind) noexcept {
    switch (runtime_kind) {
        case ManagedRuntimeKind::dotnet_framework:
            return "dotnet-framework";
        case ManagedRuntimeKind::dotnet:
            return "dotnet";
        case ManagedRuntimeKind::mono:
            return "mono";
        default:
            return "unknown";
    }
}