#include "server/cli.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace hearth::server {

namespace {

std::optional<std::uint64_t> parse_number(std::string_view text,
                                          std::uint64_t min, std::uint64_t max) {
  std::uint64_t value = 0;
  const auto *end = text.data() + text.size();
  const auto [ptr, ec] = std::from_chars(text.data(), end, value);
  if (ec != std::errc{} || ptr != end || value < min || value > max) {
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

} // namespace

std::string usage() {
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
  auto &config = outcome.config;

  for (std::size_t i = 0; i < args.size(); ++i) {
    std::string_view arg = args[i];
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
    const auto bad_value = [&] {
      return error("invalid value '" + std::string(*value) + "' for " +
                   std::string(name));
    };

    if (name == "--port") {
      const auto port = parse_number(*value, 0, 65535);
      if (!port) {
        return bad_value();
      }
      config.port = static_cast<std::uint16_t>(*port);
    } else if (name == "--host") {
      if (value->empty()) {
        return bad_value();
      }
      config.host = std::string(*value);
    } else if (name == "--threads") {
      const auto threads = parse_number(*value, 0, 1024);
      if (!threads) {
        return bad_value();
      }
      config.threads = *threads;
    } else if (name == "--root") {
      if (value->empty()) {
        return bad_value();
      }
      config.doc_root = std::string(*value);
    } else if (name == "--idle-timeout-ms") {
      const auto ms = parse_number(*value, 1, 3'600'000);
      if (!ms) {
        return bad_value();
      }
      config.idle_timeout =
          std::chrono::milliseconds(static_cast<std::int64_t>(*ms));
    } else if (name == "--max-requests") {
      const auto n =
          parse_number(*value, 1, std::numeric_limits<std::size_t>::max());
      if (!n) {
        return bad_value();
      }
      config.max_requests_per_connection = *n;
    } else if (name == "--max-body") {
      const auto n = parse_number(*value, 0, std::uint64_t{1} << 32U);
      if (!n) {
        return bad_value();
      }
      config.max_body_bytes = *n;
    } else {
      return error("unknown option '" + std::string(name) + "'");
    }
  }
  return outcome;
}

} // namespace hearth::server
