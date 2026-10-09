#pragma once

#include "http/request.hpp"
#include "http/response.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace hearth::router {

using Handler = std::function<http::Response(const http::Request &)>;

/// True if `path` falls under `prefix` on a segment boundary: "/static"
/// matches "/static" and "/static/x" but not "/staticfoo". A prefix that
/// ends in '/' (including "/") matches anything that starts with it.
bool prefix_matches(std::string_view prefix, std::string_view path) noexcept;

/// Method + path router.
///
/// Precedence: exact routes beat prefix routes; among prefix routes the
/// longest wins; ties go to the route registered first. The request is
/// dispatched to the first route in that order whose method matches. A GET
/// route also answers HEAD.
///
/// If some route matches the path but none matches the method, the answer is
/// 405 with an Allow header listing every method that would have matched.
/// If no route matches the path at all, the answer is 404.
class Router {
public:
  void add(std::string method, std::string path, Handler handler);
  void add_prefix(std::string method, std::string prefix, Handler handler);

  [[nodiscard]] http::Response dispatch(const http::Request &request) const;

private:
  struct Route {
    std::string method;
    std::string pattern;
    bool is_prefix;
    Handler handler;
  };

  std::vector<Route> routes_;
};

} // namespace hearth::router
