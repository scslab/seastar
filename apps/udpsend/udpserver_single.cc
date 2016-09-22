#include <chrono>
#include <iostream>
#include <stdexcept>

#include "core/app-template.hh"
#include "core/reactor.hh"
#include "core/seastar.hh"

#define IP_HDR_SZ 20
#define UDP_HDR_SZ 8
#define PKT_BODY_SZ 64
#define TOTAL_SZ (IP_HDR_SZ + UDP_HDR_SZ + PKT_BODY_SZ) // 92 bytes

using namespace std;
using namespace net;
using namespace std::chrono_literals;
namespace po = boost::program_options;

using ns = chrono::nanoseconds;
using get_time = steady_clock_type;

get_time::time_point ts0;
uint64_t recvd, iteration, last_recvd;
bool ts0set;

void stats_summary(void) {
  get_time::time_point ts1;
  uint64_t myrecvd, elapsed_ns;
  double mpps, mbytes;

  if (ts0set) {
    ts1 = get_time::now();
    elapsed_ns = chrono::duration_cast<ns>(ts1 - ts0).count();

    myrecvd = recvd; // grab snapshot
    
    mpps = (double(myrecvd) / double(elapsed_ns / 1000000)) * 1000;
    mpps /= double(1000000);
    mbytes = double(TOTAL_SZ * myrecvd) / double(1024 * 1024);
    mbytes = (mbytes / double(elapsed_ns / 1000000)) *1000;
    printf("\n-------------------------------------------------\n");
    printf("Summary:   %2.3f Mpps   %4.3f MBs\n", mpps, mbytes);
  } else {
    printf("Summary:   %2.3f Mpps   %4.3f MBs\n", 0.0, 0.0);
  }
}

void timer_handler(void) {
  uint64_t myrecvd, iter_recvd;
  float mpps, mbytes;

  myrecvd = recvd; // grab snapshot

  iter_recvd = myrecvd - last_recvd;
  mpps = float(iter_recvd) / float(1000000);
  mbytes = float(TOTAL_SZ * iter_recvd) / float(1024 * 1024);
  printf("%03lu:   %2.3f Mpps   %4.3f MBs\n", iteration, mpps, mbytes);

  last_recvd = myrecvd; // store snapshot
  iteration++;
}

future<> udp_server(uint16_t port) {
  return do_with(
    engine().net().make_udp_channel(make_ipv4_address({port})),
    [] (auto &server_fd) {
      return keep_doing([&server_fd] {
        return server_fd.receive().then([] (udp_datagram dgram) {
          if (not ts0set) {
            ts0 = get_time::now();
            ts0set = true;
          }
          recvd += 1;
        });
      });
    });
}

int main(int argc, char** argv) {
  int exit;
  app_template app;
  timer<> stats_timer;

  // server options
  app.add_options()
      ("port", po::value<uint16_t>()->default_value(8080), "UDP server port")
      ;

  // setup accounting
  iteration = 0;
  recvd = 0;
  last_recvd = 0;
  
  // run the server
  exit = app.run_deprecated(argc, argv, [&] {
    auto && config = app.configuration();
    uint16_t port = config["port"].as<uint16_t>();

    stats_timer.set_callback(timer_handler);
    stats_timer.arm_periodic(1s);

    udp_server(port);
  });

  stats_summary();
  return exit;
}
