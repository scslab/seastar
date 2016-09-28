#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/reactor.hh"

using namespace net;
using namespace seastar;
using namespace std::chrono_literals;

using ns = std::chrono::nanoseconds;
using get_time = steady_clock_type;

constexpr uint64_t PKT_SIZE = 24;
transport protocol = transport::TCP;
class tcp_conn;
class tcp_client;
distributed<tcp_client> clients;

class tcp_conn {
private:
  tcp_server & _server;
  connected_socket _fd;
  input_stream<char> _rx;
  output_stream<char> _tx;
  bool _reply;
  uint64_t _delay;

public:
  tcp_conn(tcp_server & server, connected_socket &&fd, bool reply, uint64_t delay)
    : _server{server}
    , _fd{std::move(fd)}
    , _rx{_fd.input()}
    , _tx{_fd.output()}
    , _reply{reply}
    , _delay{delay}
  {}

  future<> run(void) {
    return make_ready_future();
  }
};

class tcp_client {
private:
  ipv4_addr _server_addr;
  unsigned _nconns;
  bool _reply;
  uint64_t _delay;

  future<> run_client(connected_socket &&fd) {
    return do_with(new tcp_conn(*this, std::move(fd), _reply, _delay), [] (auto & conn) {
      return conn->run();
    });
  }

public:
  future<> start(ipv4_addr server_addr, unsigned nconns, bool reply, uint64_t delay) {
    _server_addr = server_addr;
    _nconns = nconns;
    _reply = reply;
    _delay = delay;

    for (unsigned i = 0; i < _nconns; i++) {
      auto addr = make_ipv4_address(_server_addr)
      socket_address local = socket_address(::sockaddr_in{AF_INET, INADDR_ANY, {0}});
      engine().net().connect(addr, local, protocol).then([this] (connected_socket fd) {
        run_client(std::move(fd));
      });
    }
    return make_ready_future();
  }

  future<> stop() {
    // TODO: Handle shutdown
    return make_ready_future();
  }
};

namespace po = boost::program_options;

int main(int ac, char ** av) {
  app_template app;
  timer<> stats_timer;

  app.add_options()
    ("server", bpo::value<std::string>(), "Server address")
    ("conn", bpo::value<unsigned>()->default_value(1), "# connections per cpu")
    ("reply", "Expect server to echo TCP packet back to client?")
    ("delay", bpo::value<uint64_t>()->default_value(0), "Sleep between samples (ns)")
    ;

  return app.run_deprecated(ac, av, [&] {
    clients.start().then([&] () mutable {
      auto&& config = app.configuration();
      auto server = config["server"].as<std::string>();
      auto ncon = config["conn"].as<unsigned>();
      bool reply = config.count("reply");
      uint64_t delay = config["delay"].as<uint64_t>();

      engine().at_exit([&] {
        return clients->stop();
      });

      clients.invoke_on_all(&tcp_client::start, ipv4_addr{server}, ncon, reply, delay);

      stats_timer.set_callback([&] {
        clients->invoke_on_all(&tcp_client::latest_stats);
      });
      stats_timer.arm_periodic(1s);
    });
  });
}
