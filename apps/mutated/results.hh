#ifndef MUTATED_RESULTS_HH
#define MUTATED_RESULTS_HH

#include <chrono>
#include <stdexcept>
#include <vector>

#include "accum.hh"
#include "clock.hh"

/**
 * Results from a sampling run.
 */
class Results
{
  private:
    mut::time_point measure_start_;
    mut::time_point measure_end_;
    Accum service_;
    Accum wait_;
    uint64_t tx_bytes_;
    uint64_t rx_bytes_;
    double reqps_;

  public:
    explicit Results(std::size_t reserve) noexcept : measure_start_{},
                                                     measure_end_{},
                                                     service_{reserve},
                                                     wait_{reserve},
                                                     tx_bytes_{0},
                                                     rx_bytes_{0},
                                                     reqps_{0}
    {
    }

    /* record results */
    void start_measurements(void) noexcept
    {
        measure_start_ = mut::clock::now();
    }
    void end_measurements(void)
    {
        measure_end_ = mut::clock::now();
        reqps_ = (double)service_.size() / (running_time_ns() / mut::NSEC);
    }
    void add_sample(uint64_t service, uint64_t wait, uint64_t rx_bytes)
    {
        service_.add_sample(service);
        wait_.add_sample(wait);
        rx_bytes_ += rx_bytes;
    }
    void add_tx(uint64_t tx_bytes) noexcept { tx_bytes_ += tx_bytes; }

    /* observe results */
    Accum::size_type size(void) const noexcept { return service_.size(); }
    uint64_t running_time_ns(void) const noexcept
    {
        auto length = measure_end_ - measure_start_;
        return std::chrono::duration_cast<mut::ns>(length).count();
    }
    const Accum &service(void) const noexcept { return service_; }
    const Accum &wait(void) const noexcept { return wait_; }
    double reqps(void) const noexcept { return reqps_; }
    uint64_t tx_bytes(void) const noexcept { return tx_bytes_; }
    uint64_t rx_bytes(void) const noexcept { return rx_bytes_; }

    /* show results */
    void print(double);
};

#endif /* MUTATED_RESULTS_HH */
