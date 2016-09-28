#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/reactor.hh"

constexpr uint64_t PKT_SIZE = 24;
constexpr uint64_t TOTAL_SZ = PKT_SIZE + 40;

using namespace net;
using namespace seastar;
using namespace std::chrono_literals;

using ns = std::chrono::nanoseconds;
using get_time = steady_clock_type;

std::tuple<double,double> tostats(uint64_t recvd, double secs) {
  double mpps = double(recvd) / secs / double(1000000);
  double mbytes = double(TOTAL_SZ * recvd) / secs / double(1024 * 1024);
  return std::make_tuple(mpps, mbytes);
}

class tcp_server {
private:
  server_socket _sock{};
  bool _reply{false};
  uint64_t _rcvd{0};
  uint64_t _sent{0};
  uint64_t _last_recvd{0};
  bool _ts0set{false};
  get_time::time_point _ts0{};

  future<> run_client(connected_socket &&fd);

public:
  void add_rcvd(void) noexcept { _rcvd++; }
  void add_sent(void) noexcept { _sent++; }
  uint64_t latest_stats(void) noexcept;
  uint64_t final_stats(void) const noexcept;

  future<> start(uint16_t port, bool reply);
  future<> stop();
};

class tcp_conn {
private:
  tcp_server & _server;
  connected_socket _fd;
  input_stream<char> _rx;
  output_stream<char> _tx;
  bool _reply;

  future<stop_iteration> pongping(void) {
    return _rx.read_exactly(PKT_SIZE).then([this] (temporary_buffer<char> buf) {
      if (buf) {
        _server.add_rcvd();
        return _tx.write(std::move(buf)).then([this] {
          _server.add_sent();
          return _tx.flush().then([] {
            return stop_iteration::no;
          });
        });
      } else {
        return make_ready_future<stop_iteration>(stop_iteration::yes);
      }
    });
  }

  future<stop_iteration> pong(void) {
    return _rx.read_exactly(PKT_SIZE).then([this] (temporary_buffer<char> buf) {
      if (buf) {
        _server.add_rcvd();
        return stop_iteration::no;
      } else {
        return stop_iteration::yes;
      }
    });
  }

public:
  tcp_conn(tcp_server & server, connected_socket &&fd, bool reply)
    : _server{server}
    , _fd{std::move(fd)}
    , _rx{_fd.input()}
    , _tx{_fd.output()}
    , _reply{reply}
  {}

  future<> run(void) {
    return repeat([this] {
      if (_reply) {
        return pongping();
      } else {
        return pong();
      }
    }).then([this] {
      return _tx.close();
    });
  }
};

future<> tcp_server::run_client(connected_socket &&fd) {
  return do_with(new tcp_conn(*this, std::move(fd), _reply), [] (auto & conn) {
    return conn->run();
  });
}

uint64_t tcp_server::latest_stats(void) noexcept {
  // uint64_t myrecvd = _rcvd;
  // uint64_t iter_recvd = myrecvd - _last_recvd;
  // auto stats = tostats(iter_recvd, 1);
  // printf("  Core %2u:  %2.3f Mpps  %4.3f MBs\n",
  //   engine().cpu_id(), std::get<0>(stats), std::get<1>(stats));
  // _last_recvd = myrecvd;
  // return iter_recvd;
  return 0;
}

uint64_t tcp_server::final_stats(void) const noexcept {
  if (not _ts0set) {
    return 0;
  }
  auto t = std::chrono::duration_cast<ns>(get_time::now() - _ts0).count();
  uint64_t myrecvd = _rcvd;
  auto stats = tostats(myrecvd, double(t) / 1000000000.0);
  printf("  Core %2u: %2.3f Mpps  %4.3f MBs\n",
    engine().cpu_id(), std::get<0>(stats),std::get<1>(stats));
  return myrecvd;
}

future<> tcp_server::start(uint16_t port, bool reply)  {
  ipv4_addr addr{port};
  listen_options lo;
  lo.proto = transport::TCP;
  lo.reuse_address = true;
  _sock = engine().listen(make_ipv4_address(addr), lo);
  _reply = reply;

  return keep_doing([this] {
    return _sock.accept().then([this] (connected_socket fd, socket_address a) {
      if (not _ts0set) {
        _ts0 = get_time::now();
        _ts0set = true;
      }
      // XXX: Don't return client future if want handle more than one at time
      return run_client(std::move(fd));
    });
  });
}

future<> tcp_server::stop() {
  // TODO: Handle shutdown
  return make_ready_future<>();
}

namespace po = boost::program_options;

int main(int ac, char** av) {
  app_template app;
  timer<> stats_timer;
  auto ts0 = get_time::now();
  uint64_t iteration = 0;
  auto server = std::make_unique<distributed<tcp_server>>();

  app.add_options()
    ("port", po::value<uint16_t>()->default_value(8080), "TCP server port")
    ("reply", "Echo TCP packet back to client?")
    ("nostats", "Don't print out stats")
    ;

  return app.run_deprecated(ac, av, [&] {
    auto&& config = app.configuration();
    uint16_t port = config["port"].as<uint16_t>();
    bool dostats = not config.count("nostats");
    bool reply = config.count("reply");

    server->start().then([&] () mutable {
      // stop
      engine().at_exit([&] {
        if (dostats) {
          printf("\n-------------------------------------------------\n");
          printf("Summary for all cores...\n");
          server->map_reduce0(
            std::mem_fn(&tcp_server::final_stats),
            0,
            std::plus<uint64_t>())
          .then([&, ts0] (uint64_t result) {
            auto ts1 = get_time::now();
            auto elapsed_ns = std::chrono::duration_cast<ns>(ts1 - ts0).count();
            auto stats = tostats(result, double(elapsed_ns) / 1000000000.0);
            printf("    Total:  %2.3f Mpps  %4.3f MBs\n",
              std::get<0>(stats), std::get<1>(stats));
          });
        }
        return server->stop();
      });

      // start
      server->invoke_on_all(&tcp_server::start, port, reply);

      // collect stats from each shard
      if (dostats) {
        stats_timer.set_callback([&] {
          printf("%03lu: Stats for all cores...\n", iteration++);
          server->map_reduce0(
            std::mem_fn(&tcp_server::latest_stats),
            0,
            std::plus<uint64_t>())
          .then([] (uint64_t result) {
            auto stats = tostats(result, 1);
            printf("    Total:  %2.3f Mpps  %4.3f MBs\n",
              std::get<0>(stats), std::get<1>(stats));
          });
        });
        stats_timer.arm_periodic(1s);
      }
    }).then([port] {
      std::cout << "TCP server listening on port " << port << " ...\n";
    });
  });
}
