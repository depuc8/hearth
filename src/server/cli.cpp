#include "server/cli.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace hearth::server {

namespace {

std::optional<std::uint64_t> parse_number(std::string_view text, std::uint64_t min,
                                          std::uint64_t max) {
  std::uint64_t value = 0;
  const char *first = text.data();
  const char *last = first + text.size();
  const auto [ptr, ec] = std::from_chars(first, last, value);
  if (ec != std::errc{} || ptr != last || value < min || value > max) {
    return std::nullopt;
  }
  return value;
}

CliOutcome error(std::string message) {
  CliOutcome outcome;
  outcome.action = CliOutcome::Action::Error;
  outcome.message = std::move(message);
  return outcome;
}

/// Applies one "--name value" pair. Returns false if the name is unknown or
/// the value is invalid; `known` tells the two apart.
bool apply_option(std::string_view name, std::string_view value, ServerConfig &config,
                  bool &known) {
  known = true;
  const auto number = [&](std::uint64_t min, std::uint64_t max) {
    return parse_number(value, min, max);
  };
  if (name == "--port") {
    const auto port = number(0, 65535);
    config.port = static_cast<std::uint16_t>(port.value_or(0));
    return port.has_value();
  }
  if (name == "--threads") {
    const auto threads = number(0, 1024);
    config.threads = threads.value_or(0);
    return threads.has_value();
  }
  if (name == "--idle-timeout-ms") {
    const auto ms = number(1, 3'600'000);
    config.idle_timeout = std::chrono::milliseconds(static_cast<std::int64_t>(ms.value_or(1)));
    return ms.has_value();
  }
  if (name == "--max-requests") {
    const auto n = number(1, std::numeric_limits<std::size_t>::max());
    config.max_requests_per_connection = n.value_or(1);
    return n.has_value();
  }
  if (name == "--max-body") {
    const auto n = number(0, std::uint64_t{1} << 32U);
    config.max_body_bytes = n.value_or(0);
    return n.has_value();
  }
  if (name == "--host") {
    config.host = std::string(value);
    return !value.empty();
  }
  if (name == "--root") {
    config.doc_root = std::string(value);
    return !value.empty();
  }
  known = false;
  return false;
}

} // namespace

std::string_view usage() {
  return "Usage: hearth [options]\n"
         "\n"
         "Options:\n"
         "  --port <n>             TCP port to listen on, 0 = ephemeral (default 8080)\n"
         "  --host <addr>          Address to bind (default 0.0.0.0)\n"
         "  --threads <n>          Worker threads, 0 = one per core (default 0)\n"
         "  --root <dir>           Document root for static files (default ./public)\n"
         "  --idle-timeout-ms <n>  Close idle keep-alive connections after n ms (default 5000)\n"
         "  --max-requests <n>     Requests per connection before closing (default 1000)\n"
         "  --max-body <bytes>     Largest accepted request body (default 1048576)\n"
         "  -h, --help             Show this help\n"
         "  -V, --version          Show version\n";
}

CliOutcome parse_cli(const std::vector<std::string> &args) {
  CliOutcome outcome;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (arg == "-h" || arg == "--help") {
      outcome.action = CliOutcome::Action::Help;
      return outcome;
    }
    if (arg == "-V" || arg == "--version") {
      outcome.action = CliOutcome::Action::Version;
      return outcome;
    }
    if (arg.substr(0, 2) != "--") {
      return error("unexpected argument '" + std::string(arg) + "'");
    }

    std::string_view name = arg;
    std::optional<std::string_view> value;
    if (const auto eq = arg.find('='); eq != std::string_view::npos) {
      name = arg.substr(0, eq);
      value = arg.substr(eq + 1);
    } else if (i + 1 < args.size()) {
      value = args[++i];
    }
    if (!value) {
      return error("option '" + std::string(name) + "' needs a value");
    }

    bool known = true;
    if (!apply_option(name, *value, outcome.config, known)) {
      return error(known ? "invalid value '" + std::string(*value) + "' for " + std::string(name)
                         : "unknown option '" + std::string(name) + "'");
    }
  }
  return outcome;
}

} // namespace hearth::server
