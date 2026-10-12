#include "server/app.hpp"

#include "static/static_handler.hpp"

namespace hearth::server {

void install_default_routes(Server &server) {
  server.router().add_prefix(
      "GET", "/", files::StaticHandler(server.config().doc_root));
}

} // namespace hearth::server
