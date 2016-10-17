#include <inttypes.h>
#include <stdio.h>

#include "clock.hh"
#include "results.hh"

using namespace std;

void Results::print(double req_s)
{
    printf("#reqs/s: hit\t\ttarget\n");
    printf("         %f\t%f\t\n\n", reqps(), req_s);

    printf("service: min\tavg\t\tstd\t\t99th\t99.9th\tmax\n");
    printf("         %" PRIu64 "\t%f\t%f\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
           "\n\n",
           service().min(), service().mean(), service().stddev(),
           service().percentile(0.99), service().percentile(0.999),
           service().max());

    printf("   wait: min\tavg\t\tstd\t\t99th\t99.9th\tmax\n");
    printf("         %" PRIu64 "\t%f\t%f\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
           "\n",
           wait().min(), wait().mean(), wait().stddev(),
           wait().percentile(0.99), wait().percentile(0.999), wait().max());

    constexpr uint64_t MB = 1024 * 1024;
    double time_s = running_time_ns() / mut::NSEC;
    double rx_mbs = double(rx_bytes()) / MB;
    double tx_mbs = double(tx_bytes()) / MB;

    printf("\n");
    printf("RX: %.2f MB/s (%.2f MB)\n", rx_mbs / time_s, rx_mbs);
    printf("TX: %.2f MB/s (%.2f MB)\n", tx_mbs / time_s, tx_mbs);
}
