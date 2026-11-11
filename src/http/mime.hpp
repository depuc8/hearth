#pragma once

#include <string_view>

namespace hearth::http {

/// Content-Type for a file path, chosen by its (case-insensitive) extension.
/// Text types include "; charset=utf-8". Unknown types are
/// "application/octet-stream".
std::string_view mime_type_for(std::string_view path) noexcept;

} // namespace hearth::http
