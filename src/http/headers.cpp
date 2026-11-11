#include "http/headers.hpp"

#include <algorithm>
#include <cstring>

namespace hearth::http {

namespace {

char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string_view trim_ows(std::string_view s) noexcept {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }
  return s;
}

} // namespace

bool iequals(std::string_view a, std::string_view b) noexcept {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return ascii_lower(x) == ascii_lower(y);
         });
}

bool is_tchar(char c) noexcept {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9')) {
    return true;
  }
  return c != '\0' && std::strchr("!#$%&'*+-.^_`|~", c) != nullptr;
}

bool list_contains_token(std::string_view value,
                         std::string_view token) noexcept {
  while (!value.empty()) {
    const auto comma = value.find(',');
    if (iequals(trim_ows(value.substr(0, comma)), token)) {
      return true;
    }
    if (comma == std::string_view::npos) {
      break;
    }
    value.remove_prefix(comma + 1);
  }
  return false;
}

void Headers::add(std::string name, std::string value) {
  items_.push_back({std::move(name), std::move(value)});
}

void Headers::set(std::string_view name, std::string value) {
  remove(name);
  items_.push_back({std::string(name), std::move(value)});
}

void Headers::remove(std::string_view name) {
  items_.erase(std::remove_if(items_.begin(), items_.end(),
                              [&](const Header &h) {
                                return iequals(h.name, name);
                              }),
               items_.end());
}

std::optional<std::string_view> Headers::get(std::string_view name) const {
  for (const auto &h : items_) {
    if (iequals(h.name, name)) {
      return std::string_view(h.value);
    }
  }
  return std::nullopt;
}

std::size_t Headers::count(std::string_view name) const {
  return static_cast<std::size_t>(
      std::count_if(items_.begin(), items_.end(),
                    [&](const Header &h) { return iequals(h.name, name); }));
}

bool Headers::has_token(std::string_view name, std::string_view token) const {
  return std::any_of(items_.begin(), items_.end(), [&](const Header &h) {
    return iequals(h.name, name) && list_contains_token(h.value, token);
  });
}

} // namespace hearth::http
