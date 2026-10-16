#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hearth::http {

/// ASCII case-insensitive comparison.
bool iequals(std::string_view a, std::string_view b) noexcept;

/// True if `c` is an RFC 9110 `tchar` (allowed in methods and field names).
bool is_tchar(char c) noexcept;

/// True if the comma-separated list `value` contains `token`
/// (case-insensitive, surrounding whitespace ignored).
bool list_contains_token(std::string_view value, std::string_view token) noexcept;

struct Header {
  std::string name;
  std::string value;
};

/// Ordered header list with case-insensitive lookup. Duplicates are kept.
class Headers {
public:
  void add(std::string name, std::string value);
  /// Replaces every header named `name` with a single one.
  void set(std::string_view name, std::string value);
  void remove(std::string_view name);

  /// The first value for `name`, if any.
  [[nodiscard]] std::optional<std::string_view> get(std::string_view name) const;
  [[nodiscard]] std::size_t count(std::string_view name) const;
  [[nodiscard]] bool contains(std::string_view name) const { return get(name).has_value(); }
  /// True if any header named `name` lists `token`.
  [[nodiscard]] bool has_token(std::string_view name, std::string_view token) const;

  [[nodiscard]] const std::vector<Header> &all() const noexcept { return items_; }
  [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
  [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
  void clear() noexcept { items_.clear(); }

private:
  std::vector<Header> items_;
};

} // namespace hearth::http
