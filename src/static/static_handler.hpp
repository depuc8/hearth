#pragma once

#include "http/request.hpp"
#include "http/response.hpp"

#include <filesystem>
#include <string_view>

namespace hearth::files {

/// Serves files from a document root.
///
/// Path-traversal protection is layered:
///  1. any ".." segment in the (already percent-decoded) path is a 403;
///  2. the resolved file is canonicalized (following symlinks) and must still
///     lie under the canonical document root, compared component by
///     component, or it is a 403.
/// Directories serve their index.html. Missing files are 404; unreadable files
/// are 403.
class StaticHandler {
public:
  /// Throws std::filesystem::filesystem_error or std::invalid_argument if
  /// `root` does not exist or is not a directory.
  explicit StaticHandler(const std::filesystem::path &root);

  http::Response operator()(const http::Request &request) const { return serve(request.path); }
  [[nodiscard]] http::Response serve(std::string_view path) const;

  [[nodiscard]] const std::filesystem::path &root() const noexcept { return root_; }

private:
  std::filesystem::path root_;
};

/// True if canonical path `p` is `root` or lies beneath it.
bool is_within(const std::filesystem::path &root, const std::filesystem::path &p) noexcept;

} // namespace hearth::files
