#include "http/mime.hpp"

#include "http/headers.hpp"

#include <array>
#include <utility>

namespace hearth::http {

namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 26> kTypes{{
    {"html", "text/html; charset=utf-8"},
    {"htm", "text/html; charset=utf-8"},
    {"css", "text/css; charset=utf-8"},
    {"js", "text/javascript; charset=utf-8"},
    {"mjs", "text/javascript; charset=utf-8"},
    {"json", "application/json"},
    {"txt", "text/plain; charset=utf-8"},
    {"md", "text/markdown; charset=utf-8"},
    {"csv", "text/csv; charset=utf-8"},
    {"xml", "application/xml"},
    {"svg", "image/svg+xml"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"webp", "image/webp"},
    {"avif", "image/avif"},
    {"ico", "image/x-icon"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"ttf", "font/ttf"},
    {"wasm", "application/wasm"},
    {"pdf", "application/pdf"},
    {"zip", "application/zip"},
    {"mp4", "video/mp4"},
    {"webm", "video/webm"},
}};

} // namespace

std::string_view mime_type_for(std::string_view path) noexcept {
  const auto slash = path.find_last_of('/');
  const auto name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  const auto dot = name.find_last_of('.');
  if (dot != std::string_view::npos && dot + 1 < name.size()) {
    const auto ext = name.substr(dot + 1);
    for (const auto &[known, type] : kTypes) {
      if (iequals(ext, known)) {
        return type;
      }
    }
  }
  return "application/octet-stream";
}

} // namespace hearth::http
