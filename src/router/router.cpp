#include "router/router.hpp"

#include <algorithm>
#include <set>

namespace hearth::router {

bool prefix_matches(std::string_view prefix, std::string_view path) noexcept {
  if (path.substr(0, prefix.size()) != prefix) {
    return false;
  }
  return path.size() == prefix.size() || (!prefix.empty() && prefix.back() == '/') ||
         path[prefix.size()] == '/';
}

void Router::add(std::string method, std::string path, Handler handler) {
  routes_.push_back({std::move(method), std::move(path), false, std::move(handler)});
}

void Router::add_prefix(std::string method, std::string prefix, Handler handler) {
  routes_.push_back({std::move(method), std::move(prefix), true, std::move(handler)});
}

http::Response Router::dispatch(const http::Request &request) const {
  std::vector<const Route *> candidates;
  for (const auto &route : routes_) {
    const bool hit = route.is_prefix ? prefix_matches(route.pattern, request.path)
                                     : route.pattern == request.path;
    if (hit) {
      candidates.push_back(&route);
    }
  }
  if (candidates.empty()) {
    return http::Response::error(http::Status::NotFound);
  }

  std::stable_sort(candidates.begin(), candidates.end(), [](const Route *a, const Route *b) {
    if (a->is_prefix != b->is_prefix) {
      return !a->is_prefix;
    }
    return a->pattern.size() > b->pattern.size();
  });

  const bool is_head = request.method == "HEAD";
  for (const Route *route : candidates) {
    if (route->method == request.method) {
      return route->handler(request);
    }
  }
  if (is_head) {
    for (const Route *route : candidates) {
      if (route->method == "GET") {
        return route->handler(request);
      }
    }
  }

  std::set<std::string> allowed;
  for (const Route *route : candidates) {
    allowed.insert(route->method);
    if (route->method == "GET") {
      allowed.insert("HEAD");
    }
  }
  std::string allow;
  for (const auto &method : allowed) {
    if (!allow.empty()) {
      allow += ", ";
    }
    allow += method;
  }
  auto response = http::Response::error(http::Status::MethodNotAllowed);
  response.headers.set("Allow", std::move(allow));
  return response;
}

} // namespace hearth::router
