#include "http/url.hpp"

namespace hearth::http {

namespace {

int hex_value(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

} // namespace

std::optional<std::string> percent_decode(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c == '%') {
      if (i + 2 >= input.size()) {
        return std::nullopt;
      }
      const int hi = hex_value(input[i + 1]);
      const int lo = hex_value(input[i + 2]);
      if (hi < 0 || lo < 0) {
        return std::nullopt;
      }
      c = static_cast<char>((hi << 4) | lo);
      i += 2;
    }
    if (c == '\0') {
      return std::nullopt;
    }
    out.push_back(c);
  }
  return out;
}

} // namespace hearth::http
