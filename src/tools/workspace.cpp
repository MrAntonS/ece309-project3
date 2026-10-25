// src/tools/workspace.cpp — lexical workspace containment (spec §4.4).
#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains a NUL byte";
        return false;
    }

    // A lexical check only; check_symlinks() performs the separate OS check.
    try {
        const fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }
        const fs::path normalized = p.lexically_normal();
        if (normalized.begin() != normalized.end() && *normalized.begin() == "..") {
            *reason = "path escapes the workspace";
            return false;
        }
        *out = (normalized == fs::path(".")) ? root() : root() / normalized;
        reason->clear();
        return true;
    } catch (...) {
        *reason = "invalid path";
        return false;
    }
}
