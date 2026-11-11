#pragma once

#include "server/config.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace hearth::server {

struct CliOutcome {
  enum class Action { Run, Help, Version, Error };
  Action action = Action::Run;
  ServerConfig config;
  /// Error description when action == Error.
  std::string message;
};

/// Parses command-line arguments (excluding argv[0]). Accepts both
/// "--port 8080" and "--port=8080".
CliOutcome parse_cli(const std::vector<std::string> &args);

/// Help text for --help.
std::string_view usage();

} // namespace hearth::server
