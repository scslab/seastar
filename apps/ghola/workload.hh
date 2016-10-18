#ifndef GHOLA_WORKLOAD_HH
#define GHOLA_WORKLOAD_HH

#include <chrono>
#include <cstdint>

/**
 * A 'workload' is just a block of memory we'll touch to simulate a working
 * sets cache footprint.
 */
class Workload
{
private:
    static double ITER_PER_US_;
    uint64_t dlines_;
    char *working_set_;

    double _workload_calibrate(void);
    void _run_for_n(uint64_t us);

public:
    explicit Workload(uint64_t lines);
    ~Workload(void);

    Workload(const Workload &) = delete;
    Workload &operator=(const Workload &) = delete;
    Workload(Workload &&) = delete;
    Workload &operator=(Workload &&) = delete;

    void run_for_mem(std::chrono::microseconds d);
    void run_for_spin(std::chrono::microseconds d);
    void run_for_sleep(std::chrono::microseconds d);
};

#endif /* GHOLA_WORKLOAD_HH */
