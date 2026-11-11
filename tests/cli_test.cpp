#include "server/cli.hpp"

#include <gtest/gtest.h>

using hearth::server::CliOutcome;
using hearth::server::parse_cli;

TEST(Cli, DefaultsWithNoArguments) {
  const auto out = parse_cli({});
  ASSERT_EQ(out.action, CliOutcome::Action::Run);
  EXPECT_EQ(out.config.port, 8080);
  EXPECT_EQ(out.config.threads, 0U);
  EXPECT_EQ(out.config.doc_root, "./public");
  EXPECT_EQ(out.config.idle_timeout, std::chrono::milliseconds(5000));
}

TEST(Cli, ParsesDocumentedFlags) {
  const auto out = parse_cli({"--port", "9090", "--threads", "8", "--root", "/srv/www"});
  ASSERT_EQ(out.action, CliOutcome::Action::Run) << out.message;
  EXPECT_EQ(out.config.port, 9090);
  EXPECT_EQ(out.config.threads, 8U);
  EXPECT_EQ(out.config.doc_root, "/srv/www");
}

TEST(Cli, ParsesEqualsFormAndExtras) {
  const auto out = parse_cli({"--port=0", "--host=127.0.0.1", "--idle-timeout-ms=250",
                              "--max-requests=10", "--max-body=2048"});
  ASSERT_EQ(out.action, CliOutcome::Action::Run) << out.message;
  EXPECT_EQ(out.config.port, 0);
  EXPECT_EQ(out.config.host, "127.0.0.1");
  EXPECT_EQ(out.config.idle_timeout, std::chrono::milliseconds(250));
  EXPECT_EQ(out.config.max_requests_per_connection, 10U);
  EXPECT_EQ(out.config.max_body_bytes, 2048U);
}

TEST(Cli, HelpAndVersion) {
  EXPECT_EQ(parse_cli({"--help"}).action, CliOutcome::Action::Help);
  EXPECT_EQ(parse_cli({"-h"}).action, CliOutcome::Action::Help);
  EXPECT_EQ(parse_cli({"--version"}).action, CliOutcome::Action::Version);
  EXPECT_NE(hearth::server::usage().find("--threads"), std::string::npos);
}

TEST(Cli, RejectsBadNumbers) {
  for (const auto &args : std::vector<std::vector<std::string>>{{"--port", "70000"},
                                                                {"--port", "-1"},
                                                                {"--port", "80x"},
                                                                {"--threads", "abc"},
                                                                {"--idle-timeout-ms", "0"},
                                                                {"--max-requests", "0"}}) {
    const auto out = parse_cli(args);
    EXPECT_EQ(out.action, CliOutcome::Action::Error) << args[0] << " " << args[1];
    EXPECT_FALSE(out.message.empty());
  }
}

TEST(Cli, RejectsUnknownAndIncompleteOptions) {
  EXPECT_EQ(parse_cli({"--bogus", "1"}).action, CliOutcome::Action::Error);
  EXPECT_EQ(parse_cli({"--port"}).action, CliOutcome::Action::Error);
  EXPECT_EQ(parse_cli({"positional"}).action, CliOutcome::Action::Error);
  EXPECT_EQ(parse_cli({"--root="}).action, CliOutcome::Action::Error);
}
