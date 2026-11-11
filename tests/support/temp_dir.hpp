#pragma once

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <stdlib.h> // mkdtemp

namespace hearth::test {

/// A uniquely named directory under the system temp dir, removed on scope exit.
class TempDir {
public:
  TempDir() {
    std::string pattern =
        (std::filesystem::temp_directory_path() / "hearth-test-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
      throw std::system_error(errno, std::generic_category(), "mkdtemp");
    }
    path_ = pattern;
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TempDir(const TempDir &) = delete;
  TempDir &operator=(const TempDir &) = delete;
  TempDir(TempDir &&) = delete;
  TempDir &operator=(TempDir &&) = delete;

  [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  /// Writes `content` to `relative` (creating parent directories).
  std::filesystem::path write(const std::filesystem::path &relative,
                              std::string_view content) const {
    const auto full = path_ / relative;
    std::filesystem::create_directories(full.parent_path());
    std::ofstream(full, std::ios::binary) << content;
    return full;
  }

private:
  std::filesystem::path path_;
};

} // namespace hearth::test
