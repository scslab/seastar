#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/future-util.hh"
#include "core/reactor.hh"
#include "core/sleep.hh"
#include "net/api.hh"

using namespace net;
using namespace std::chrono_literals;

#define PKT_BODY_SZ 64
char data[PKT_BODY_SZ];

class udp_client {
private:
  bool _run = false;
  promise<> _done {};
  udp_channel _sock {};
  ipv4_addr _server_addr {};
  uint64_t _sent {};
  uint64_t _errs {};

  bool stop_running(void) const noexcept {
    return not _run;
  }

public:
  void latest_stats(void) {
      std::cout << "Core " << engine().cpu_id() << ": ";
      std::cout << "Out: " << _sent << " pps,\t";
      std::cout << "Err: " << _errs << " pps\n";
      _sent = 0;
      _errs = 0;
  }

  void start(ipv4_addr server_addr) {
    _sock = engine().net().make_udp_channel();
    _server_addr = server_addr;
    _run = true;

    // we send one packet first and then wait to allow ARP to complete
    _sock.send(_server_addr, packet::from_static_data(data, PKT_BODY_SZ)).then([this] {
      sleep(5s).then([this] {
        auto stop = std::bind(&udp_client::stop_running, this);
        do_until(stop, [this] {
          return _sock.send(_server_addr, packet::from_static_data(data, PKT_BODY_SZ))
            .then_wrapped([this] (auto&& f) {
              try {
                f.get();
                _sent++;
              } catch (...) {
                _errs++;
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

  // client options
  app.add_options()
      ("server", bpo::value<std::string>(), "Server address")
      ;

  // run seastar
  return app.run_deprecated(ac, av, [&] {
    client->start().then([&] () mutable {
      // on exit
      engine().at_exit([&] {
        return client->stop();
      });

      // get config
      auto&& config = app.configuration();
      ipv4_addr addr = config["server"].as<std::string>();

      // start
      client->invoke_on_all(&udp_client::start, addr);

      // stats timer
      stats_timer.set_callback([&] {
        client->invoke_on_all(&udp_client::latest_stats);
      });
      stats_timer.arm_periodic(1s);
    });
  });
}
