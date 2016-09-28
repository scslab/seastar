#include <chrono>
#include <iostream>
#include <stdexcept>

#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/future-util.hh"
#include "core/seastar.hh"

#define IP_HDR_SZ 20
#define UDP_HDR_SZ 8
#define PKT_BODY_SZ 64
#define TOTAL_SZ (IP_HDR_SZ + UDP_HDR_SZ + PKT_BODY_SZ) // 92 bytes

using namespace net;
using namespace std::chrono_literals;
namespace po = boost::program_options;

using ns = std::chrono::nanoseconds;
using get_time = steady_clock_type;

std::tuple<double,double> tostats(uint64_t recvd, double secs) {
  double mpps = double(recvd) / secs / double(1000000);
  double mbytes = double(TOTAL_SZ * recvd) / secs / double(1024 * 1024);
  return std::make_tuple(mpps, mbytes);
}

class udp_server {
private:
  bool _run;
  promise<> _done;
  udp_channel _sock;
  timer<> _stats_timer;
  get_time::time_point _ts0;
  uint64_t _rcvd;
  uint64_t _sent;
  uint64_t _last_recvd;
  bool _reply;
  bool _ts0set;

  bool stop_running(void) const noexcept {
    return not _run;
  }

public:
  udp_server(void) noexcept
    : _run{false}, _done{}, _sock{}, _stats_timer{}, _ts0{}, _rcvd{0}
    ,  _last_recvd{0}, _reply{false}, _ts0set{false}
  {}

  uint64_t get_recvd(void) const noexcept {
    return _rcvd;
  }

  uint64_t latest_stats(void) noexcept {
    uint64_t myrecvd, iter_recvd;

    myrecvd = _rcvd; // grab snapshot
    iter_recvd = myrecvd - _last_recvd;
    auto stats = tostats(iter_recvd, 1);
    printf("  Core %2u:  %2.3f Mpps  %4.3f MBs\n",
      engine().cpu_id(), std::get<0>(stats), std::get<1>(stats));
    _last_recvd = myrecvd; // store snapshot

    return iter_recvd;
  }

  uint64_t final_stats(void) const noexcept {
    uint64_t myrecvd, elapsed_ns;

    if (not _ts0set) {
      return 0;
    }

    auto ts1 = get_time::now();
    elapsed_ns = std::chrono::duration_cast<ns>(ts1 - _ts0).count();
    myrecvd = _rcvd;

    auto stats = tostats(myrecvd, double(elapsed_ns) / 1000000000.0);
    printf("  Core %2u: %2.3f Mpps  %4.3f MBs\n",
      engine().cpu_id(), std::get<0>(stats),std::get<1>(stats));

    return myrecvd;
  }

  void start(uint16_t port, bool reply) {
    _sock = engine().net().make_udp_channel(make_ipv4_address({port}));
    auto stop = std::bind(&udp_server::stop_running, this);
    _run = true;
    _reply = reply;

    do_until(stop, [this] {
      return _sock.receive().then([this] (udp_datagram dgram) {
        if (not _ts0set) {
          _ts0 = get_time::now();
          _ts0set = true;
        }
        _rcvd++;
        if (_reply) {
          return _sock.send(dgram.get_src(), std::move(dgram.get_data())).then([this] {
            _sent++;
          });
        } else {
          return make_ready_future<>();
        }
      });
    }).finally([this] {
      _done.set_value();
    }).handle_exception([] (std::exception_ptr ex) {
      // TODO: should probably check it's a closed socket exception, otherwise
      // log the error
      return;
    });
  }

  future<> stop() {
    if (_run) {
      _run = false;              // signal server to exit
      _sock.close();             // close socket to exit rcv loop
      return _done.get_future(); // wait for exit
    } else {
      return make_ready_future<>();
    }
  }
};

int main(int ac, char ** av) {
  uint64_t iteration = 0;
  app_template app;
  timer<> stats_timer;
  auto server = std::make_unique<distributed<udp_server>>();
  auto ts0 = get_time::now();

  // server options
  app.add_options()
    ("port", po::value<uint16_t>()->default_value(8080), "UDP server port")
    ("reply", "Echo UDP packet back to client?")
    ("nostats", "Don't print out stats")
    ;

  // run seastar
  return app.run_deprecated(ac, av, [&] {
    // get config
    auto&& config = app.configuration();
    uint16_t port = config["port"].as<uint16_t>();
    bool reply = config.count("reply");
    bool dostats = not config.count("nostats");

    // run server
    server->start().then([&] () {
      // on exit
      engine().at_exit([&] {
        if (dostats) {
          printf("\n-------------------------------------------------\n");
          printf("Summary for all cores...\n");
          server->map_reduce0(
            std::mem_fn(&udp_server::final_stats),
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
      server->invoke_on_all(&udp_server::start, port, reply);

      // collect stats from each shard
      if (dostats) {
        stats_timer.set_callback([&] {
          printf("%03lu: Stats for all cores...\n", iteration++);
          server->map_reduce0(
            std::mem_fn(&udp_server::latest_stats),
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
      std::cout << "UDP server listening on port " << port << " ...\n";
    });
  });
}
