#include "static/static_handler.hpp"

#include "http/mime.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace hearth::files {

namespace fs = std::filesystem;
using http::Response;
using http::Status;

namespace {

/// Splits a URL path into segments, rejecting "..". Returns false on "..".
bool relative_from_url(std::string_view path, fs::path &out) {
  while (!path.empty()) {
    const auto slash = path.find('/');
    const auto segment = path.substr(0, slash);
    if (segment == "..") {
      return false;
    }
    if (!segment.empty() && segment != ".") {
      out /= fs::path(std::string(segment));
    }
    if (slash == std::string_view::npos) {
      break;
    }
    path.remove_prefix(slash + 1);
  }
  return true;
}

Status status_for(const std::error_code &ec) {
  return ec == std::errc::permission_denied ? Status::Forbidden : Status::NotFound;
}

} // namespace

bool is_within(const fs::path &root, const fs::path &p) noexcept {
  const auto mismatch = std::mismatch(root.begin(), root.end(), p.begin(), p.end());
  return mismatch.first == root.end();
}

StaticHandler::StaticHandler(const fs::path &root) : root_(fs::canonical(root)) {
  if (!fs::is_directory(root_)) {
    throw std::invalid_argument("document root is not a directory: " + root.string());
  }
}

Response StaticHandler::serve(std::string_view path) const {
  if (path.empty() || path.front() != '/') {
    return Response::error(Status::NotFound);
  }
  fs::path relative;
  if (!relative_from_url(path, relative)) {
    return Response::error(Status::Forbidden);
  }

  std::error_code ec;
  fs::path resolved = fs::canonical(root_ / relative, ec);
  if (ec) {
    return Response::error(status_for(ec));
  }
  if (!is_within(root_, resolved)) {
    return Response::error(Status::Forbidden);
  }
  if (fs::is_directory(resolved, ec)) {
    resolved = fs::canonical(resolved / "index.html", ec);
    if (ec) {
      return Response::error(status_for(ec));
    }
    if (!is_within(root_, resolved)) {
      return Response::error(Status::Forbidden);
    }
  }
  if (!fs::is_regular_file(resolved, ec)) {
    return Response::error(Status::NotFound);
  }

  std::ifstream file(resolved, std::ios::binary);
  if (!file) {
    return Response::error(Status::Forbidden);
  }
  std::string body{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  if (file.bad()) {
    return Response::error(Status::InternalServerError);
  }
  return Response::text(Status::Ok, std::move(body),
                        http::mime_type_for(resolved.filename().string()));
}

} // namespace hearth::files
