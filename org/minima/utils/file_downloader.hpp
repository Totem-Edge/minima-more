#pragma once

#include <filesystem>
#include <string>

namespace org {
namespace minima {
namespace utils {

// Download an http(s) URL to a file (streaming; no 1 MiB RPC cap). Writes
// <dest>.part and renames on success. Applies scheme, redirect, size and
// low-speed limits. Returns true on success.
bool downloadFile(const std::string& url,
                  const std::filesystem::path& dest,
                  long long maxBytes = 8LL * 1024 * 1024 * 1024);

} // namespace utils
} // namespace minima
} // namespace org
