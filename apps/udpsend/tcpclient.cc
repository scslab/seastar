#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/reactor.hh"
#include "core/sleep.hh"

#include "accum.hh"

using namespace net;
using namespace seastar;
using namespace std::chrono_literals;

using ns = std::chrono::nanoseconds;
using get_time = steady_clock_type;

transport protocol = transport::TCP;
class tcp_conn;
class tcp_client;
constexpr uint64_t PKT_SIZE = 24;
char data[PKT_SIZE];

class tcp_client {
private:
  ipv4_addr _server_addr{};
  unsigned _nconns{1};
  bool _reply{};
  uint64_t _delay{};
  uint64_t _sent{};
  uint64_t _errs{};
  uint64_t _rcvd{};
  get_time::time_point _ts0{};
  accum _stats{};

  future<> run_client(connected_socket &&fd);

public:
  void add_sent(void) noexcept { _sent++; }
  void add_rcvd(void) noexcept { _rcvd++; }
  void add_errs(void) noexcept { _errs++; }
  void add_rtt(uint64_t t_ns) { _stats.add(t_ns); }

  void latest_stats(void) {
    std::cout << "Core " << engine().cpu_id() << ": ";
    std::cout << "Out: " << _sent << " pps, ";
    std::cout << "Err: " << _errs << " pps\n";
    _sent = 0;
    _errs = 0;
    if (_reply) {
      std::cout << "        Avg (ns): " << _stats.average();
      std::cout << ", Dev (ns): " << _stats.stddev();
      std::cout << ", Min (ns): " << _stats.min();
      std::cout << ", Max (ns): " << _stats.max();
      std::cout << "\n";
      _stats.clear();
    }
  }

  future<> start(ipv4_addr server_addr, unsigned nconns, bool reply, uint64_t delay) {
    _server_addr = server_addr;
    _nconns = nconns;
    _reply = reply;
    _delay = delay;

    for (unsigned i = 0; i < _nconns; i++) {
      auto addr = make_ipv4_address(_server_addr);
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

class tcp_conn {
private:
  tcp_client & _client;
  connected_socket _fd;
  input_stream<char> _rx;
  output_stream<char> _tx;
  bool _reply;
  uint64_t _delay;
  get_time::time_point _ts0;

  void save_rtt(void) {
    auto ts1 = get_time::now();
    auto t_ns = std::chrono::duration_cast<ns>(ts1 - _ts0).count();
    _client.add_rtt(t_ns);
  }

  future<stop_iteration> continue_with_delay(void) {
    if (_delay > 0) {
      return sleep(std::chrono::nanoseconds(_delay)).then([] {
        return stop_iteration::no;
      });
    } else {
      return make_ready_future<stop_iteration>(stop_iteration::no);
    }
  }

  future<stop_iteration> pingpong(void) {
    _ts0 = get_time::now();
    return _tx.write(data, PKT_SIZE).then_wrapped([this] (auto&& f) {
      try {
        f.get();
        _client.add_sent();
      } catch (...) {
        _client.add_errs();
      }
      return _tx.flush().then([this] {
        return _rx.read_exactly(PKT_SIZE).then([this] (temporary_buffer<char> buf) {
          if (buf) {
            _client.add_rcvd();
            this->save_rtt();
            return this->continue_with_delay();
          } else {
            return make_ready_future<stop_iteration>(stop_iteration::yes);
          }
        });
      });
    });
  }

  future<stop_iteration> ping(void) {
    return _tx.write(data, PKT_SIZE).then_wrapped([this] (auto&& f) {
      try {
        f.get();
        _client.add_sent();
      } catch (...) {
        _client.add_errs();
      }
      return _tx.flush().then([] {
        return stop_iteration::no;
      });
    });
  }

public:
  tcp_conn(tcp_client & client, connected_socket &&fd, bool reply, uint64_t delay)
    : _client{client}
    , _fd{std::move(fd)}
    , _rx{_fd.input()}
    , _tx{_fd.output()}
    , _reply{reply}
    , _delay{delay}
    , _ts0{}
  {
    _fd.set_nodelay(true);
  }

  future<> run(void) {
    return repeat([this] {
      if (_reply) {
        return pingpong();
      } else {
        return ping();
      }
    }).then([this] {
      return _tx.close();
    });
  }
};

future<> tcp_client::run_client(connected_socket &&fd) {
  return do_with(new tcp_conn(*this, std::move(fd), _reply, _delay), [] (auto & conn) {
    return conn->run();
  });
}

namespace po = boost::program_options;

int main(int ac, char ** av) {
  app_template app;
  timer<> stats_timer;
  auto clients = std::make_unique<distributed<tcp_client>>();

  app.add_options()
    ("server", po::value<std::string>(), "Server address")
    ("conn", po::value<unsigned>()->default_value(1), "# connections per cpu")
    ("reply", "Expect server to echo TCP packet back to client?")
    ("delay", po::value<uint64_t>()->default_value(0), "Sleep between samples (ns)")
    ;

  return app.run_deprecated(ac, av, [&] {
    clients->start().then([&] () mutable {
      auto&& config = app.configuration();
      auto server = config["server"].as<std::string>();
      auto ncon = config["conn"].as<unsigned>();
      bool reply = config.count("reply");
      uint64_t delay = config["delay"].as<uint64_t>();

      engine().at_exit([&] {
        return clients->stop();
      });

      clients->invoke_on_all(&tcp_client::start, ipv4_addr{server}, ncon, reply, delay);

      stats_timer.set_callback([&] {
        clients->invoke_on_all(&tcp_client::latest_stats);
      });
      stats_timer.arm_periodic(1s);
    });
  });
}
