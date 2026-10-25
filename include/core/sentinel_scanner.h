// include/core/sentinel_scanner.h
// YOU IMPLEMENT — streaming sentinel detector (spec §3.4).
#pragma once
#include <string>
#include <string_view>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel);

    struct Out {
        std::string safe_text;
        bool sentinel_found;
    };

    Out feed(std::string_view chunk);
    Out flush();

    // Test hook for the bounded-memory requirement: number of chars
    // currently held back (must stay <= sentinel.size() - 1).
    std::size_t pending_size() const noexcept { return pending_.size(); }

private:
    std::string sentinel_;
    std::string pending_;
    bool found_ = false;
};
