#pragma once

#include "server/server.hpp"

namespace hearth::server {

/// Registers hearth's built-in routes on `server`:
///   GET /stats  metrics as JSON
///   GET /*      static files from config().doc_root
/// Throws if the document root is missing.
void install_default_routes(Server &server);

} // namespace hearth::server
