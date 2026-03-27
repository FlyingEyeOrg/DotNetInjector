#include <iostream>
#include <string>

#include "managed_runtime_resolver.h"

namespace {

int expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << std::endl;
        return 1;
    }

    return 0;
}

}  // namespace

int main() {
    int failures = 0;

    failures += expect(parse_runtime_hint(L".NetCore") == ManagedRuntimeKind::dotnet,
                       "parse .NetCore -> dotnet");
    failures += expect(parse_runtime_hint(L"Mono") == ManagedRuntimeKind::mono,
                       "parse Mono -> mono");
    failures += expect(parse_runtime_hint(L"v4.0.30319") == ManagedRuntimeKind::dotnet_framework,
                       "parse framework version -> dotnet_framework");
    failures += expect(resolve_runtime_kind(L"", RuntimePresence{true, false, false}) == ManagedRuntimeKind::dotnet,
                       "detect coreclr -> dotnet");
    failures += expect(resolve_runtime_kind(L"", RuntimePresence{false, true, false}) == ManagedRuntimeKind::dotnet_framework,
                       "detect mscoree -> dotnet_framework");
    failures += expect(resolve_runtime_kind(L"", RuntimePresence{false, false, true}) == ManagedRuntimeKind::mono,
                       "detect mono -> mono");
    failures += expect(resolve_runtime_kind(L"Mono", RuntimePresence{true, true, false}) == ManagedRuntimeKind::mono,
                       "hint should override loaded runtime");

    if (failures != 0) {
        return failures;
    }

    std::cout << "RuntimeDispatchTests passed." << std::endl;
    return 0;
}