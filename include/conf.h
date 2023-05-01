#pragma once
namespace kvs
{
constexpr static const char *kDefaultTestDir = "./engine_test/";
constexpr static const char *kDefaultBenchDir = "./engine_bench/";
constexpr static size_t kMaxKeySize = 4 * 1024;     // 4KB (2^12)
constexpr static size_t kMaxValueSize = 16 * 1024;  // 16MB (2^24)
constexpr static size_t kMaxKeyValueSize = std::max(kMaxKeySize, kMaxValueSize);
}  // namespace kvs