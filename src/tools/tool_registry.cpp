// src/tools/tool_registry.cpp — separate-chaining registry (spec §4.3).
#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(16) {}
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // 64-bit FNV-1a, not reduced to a bucket index until insertion/lookup.
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> next(new_bucket_count);
    for (Bucket& bucket : buckets_) {
        for (auto& tool : bucket) {
            const std::size_t i = hash(tool->name()) % new_bucket_count;
            next[i].push_back(std::move(tool));
        }
    }
    buckets_ = std::move(next);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;
    const std::string key = tool->name();
    if (find(key) != nullptr) return false;  // Destroys only the duplicate.

    // Keep load factor <= 0.75. Do this BEFORE the insertion.
    if (static_cast<double>(size_ + 1) / buckets_.size() > 0.75)
        rehash(buckets_.size() * 2);

    buckets_[hash(key) % buckets_.size()].push_back(std::move(tool));
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const Bucket& bucket = buckets_[hash(name) % buckets_.size()];
    for (const auto& tool : bucket)
        if (tool->name() == name) return tool.get();
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept { return size_; }

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const Bucket& bucket : buckets_)
        for (const auto& tool : bucket) fn(*tool);
}

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t worst = 0;
    for (const Bucket& bucket : buckets_)
        if (bucket.size() > worst) worst = bucket.size();
    return worst;
}
