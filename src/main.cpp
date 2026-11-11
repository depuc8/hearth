#include "server/app.hpp"
#include "server/cli.hpp"
#include "server/server.hpp"
#include "version.hpp"

#include <csignal>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

namespace {

using hearth::server::CliOutcome;

/// Blocks SIGINT/SIGTERM in the calling thread; threads spawned afterwards
/// inherit the mask, so only the dedicated waiter thread ever sees them.
sigset_t block_shutdown_signals() {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);
  return set;
}

} // namespace

int main(int argc, char **argv) {
  const auto cli =
      hearth::server::parse_cli(std::vector<std::string>(argv + 1, argv + argc));
  switch (cli.action) {
  case CliOutcome::Action::Help:
    std::cout << hearth::server::usage();
    return 0;
  case CliOutcome::Action::Version:
    std::cout << "hearth " << hearth::version() << '\n';
    return 0;
  case CliOutcome::Action::Error:
    std::cerr << "hearth: " << cli.message << "\n\n" << hearth::server::usage();
    return 2;
  case CliOutcome::Action::Run:
    break;
  }

  std::signal(SIGPIPE, SIG_IGN);
  const sigset_t signals = block_shutdown_signals();

  try {
    hearth::server::Server server(cli.config);
    hearth::server::install_default_routes(server);

    std::thread signal_waiter([&server, signals] {
      int received = 0;
      sigwait(&signals, &received);
      server.stop();
    });

    std::cout << "hearth " << hearth::version() << " listening on http://"
              << cli.config.host << ':' << server.port() << " ("
              << hearth::server::resolve_thread_count(cli.config.threads)
              << " threads, root " << cli.config.doc_root << ")" << std::endl;
    try {
      server.run();
    } catch (...) {
      ::kill(::getpid(), SIGTERM); // release the waiter thread
      signal_waiter.join();
      throw;
    }
    // run() only returns after stop(), i.e. after the waiter has fired.
    signal_waiter.join();
    std::cout << "hearth: shut down cleanly" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "hearth: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
