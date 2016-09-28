#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/future-util.hh"
#include "core/reactor.hh"
#include "core/sleep.hh"
#include "net/api.hh"

#include "accum.hh"

using namespace net;
using namespace std::chrono_literals;

constexpr uint64_t PKT_SIZE = 24;
char data[PKT_SIZE];

using ns = std::chrono::nanoseconds;
using get_time = steady_clock_type;

class udp_client {
private:
  bool _run{false};
  promise<> _done{};
  udp_channel _sock{};
  bool _reply{false};
  uint64_t _delay{0};
  ipv4_addr _server_addr{};
  uint64_t _sent{};
  uint64_t _errs{};
  uint64_t _rcvd{};
  bool _tsCorrupted{true};
  get_time::time_point _ts0{};
  accum _stats{};

  bool stop_running(void) const noexcept {
    return not _run;
  }

public:
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

    // we clear the timestamp if in progress to avoid a corrupt value
    _tsCorrupted = true;
  }

  void start(ipv4_addr server_addr, uint64_t delay, bool reply) {
    _sock = engine().net().make_udp_channel();
    _server_addr = server_addr;
    _reply = reply;
    _delay = delay;
    _run = true;

    // we send one packet first and then wait to allow ARP to complete
    _sock.send(_server_addr, packet::from_static_data(data, PKT_SIZE)).then([this] {
      sleep(5s).then([this] {
        auto stop = std::bind(&udp_client::stop_running, this);
        do_until(stop, [this] {
          _ts0 = get_time::now();
          _tsCorrupted = false;
          return _sock.send(_server_addr, packet::from_static_data(data, PKT_SIZE))
            .then_wrapped([this] (auto&& f) {
              try {
                f.get();
                _sent++;
              } catch (...) {
                _errs++;
              }
              if (_reply) {
                return _sock.receive().then([this] (udp_datagram dgram) {
                  _rcvd++;
                  if (not _tsCorrupted) {
                    auto ts1 = get_time::now();
                    uint64_t elapsed_ns = std::chrono::duration_cast<ns>(ts1 - _ts0).count();
                    _stats.add(elapsed_ns);
                  }
                  if (_delay > 0) {
                    return sleep(std::chrono::nanoseconds(_delay));
                  } else {
                    return make_ready_future<>();
                  }
                });
              } else {
                if (_delay > 0) {
                  return sleep(std::chrono::nanoseconds(_delay));
                } else {
                  return make_ready_future<>();
                }
              }
            });
        }).finally([this] {
          _done.set_value();
        }).handle_exception([] (std::exception_ptr ex) {
          // TODO: should probably check it's a closed socket exception, otherwise
          // log the error
          return;
        });
      });
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

namespace bpo = boost::program_options;

int main(int ac, char ** av) {
  app_template app;
  timer<> stats_timer;
  auto client = std::make_unique<distributed<udp_client>>();

  app.add_options()
      ("server", bpo::value<std::string>(), "Server address")
      ("reply", "Expect server to echo UDP packet back to client?")
      ("delay", bpo::value<uint64_t>()->default_value(0), "Sleep between samples (ns)")
      ;

  return app.run_deprecated(ac, av, [&] {
    client->start().then([&] () mutable {
      auto&& config = app.configuration();
      ipv4_addr addr = config["server"].as<std::string>();
      bool reply = config.count("reply");
      uint64_t delay = config["delay"].as<uint64_t>();

      engine().at_exit([&] {
        return client->stop();
      });

      client->invoke_on_all(&udp_client::start, addr, delay, reply);

      stats_timer.set_callback([&] {
        client->invoke_on_all(&udp_client::latest_stats);
      });
      stats_timer.arm_periodic(1s);
    });
  });
}
