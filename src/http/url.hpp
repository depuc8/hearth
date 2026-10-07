#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hearth::http {

/// Decodes %XX escapes. Returns nullopt for malformed escapes (e.g. "%2", "%zz")
/// or if the result would contain a NUL byte. '+' is left as is: it only
/// means space in form bodies, not in paths.
std::optional<std::string> percent_decode(std::string_view input);

} // namespace hearth::http
