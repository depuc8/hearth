#include "server/app.hpp"

#include "static/static_handler.hpp"

namespace hearth::server {

void install_default_routes(Server &server) {
  server.router().add("GET", "/stats", [&metrics = server.metrics()](const http::Request &) {
    auto response = http::Response::text(http::Status::Ok, metrics.to_json(), "application/json");
    response.headers.set("Cache-Control", "no-store");
    return response;
  });
  server.router().add_prefix("GET", "/", files::StaticHandler(server.config().doc_root));
}

} // namespace hearth::server
