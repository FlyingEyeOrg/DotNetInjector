#pragma once

#include <string>

enum class ManagedRuntimeKind {
    unknown = 0,
    dotnet_framework,
    dotnet,
    mono,
};

struct RuntimePresence {
    bool has_coreclr{};
    bool has_mscoree{};
    bool has_mono{};
};

ManagedRuntimeKind parse_runtime_hint(const std::wstring& framework_version_hint);
ManagedRuntimeKind resolve_runtime_kind(const std::wstring& framework_version_hint,
                                        const RuntimePresence& presence);
RuntimePresence detect_runtime_presence();
const char* to_string(ManagedRuntimeKind runtime_kind) noexcept;