#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

#include "workload.hh"

using namespace std;

static constexpr uint64_t CALIBRATE_N = 50000000;
double Workload::ITER_PER_US_ = 0;

/**
 * Primitive PRNG - just need to touch 'random' cache lines, doesn't need good
 * randomness properties.
 */
static inline uint64_t __mm_crc32_u64(uint64_t crc, uint64_t val)
{
    asm("crc32q %1, %0" : "+r"(crc) : "rm"(val));
    return crc;
}

/**
 * A 'Workload' is just a block of memory we'll touch to simulate a working
 * sets cache footprint.
 */
Workload::Workload(uint64_t lines)
    : dlines_{lines * 64}
    , working_set_{(char*)malloc(sizeof(char) * dlines_)}
{
    if (working_set_ == nullptr) {
        throw bad_alloc();
    }
    if (ITER_PER_US_ == 0) {
        // OK to race - should be idempotent
        ITER_PER_US_ = _workload_calibrate();
    }
}

Workload::~Workload(void)
{
    if (working_set_ != nullptr) {
        free(working_set_);
        working_set_ = nullptr;
    }
}

double Workload::_workload_calibrate(void)
{
    auto t0 = chrono::steady_clock::now();
    _run_for_n(CALIBRATE_N);
    auto t1 = chrono::steady_clock::now();

    uint64_t delta_us = chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
    double iter_per_us = double(CALIBRATE_N) / double(delta_us);
    cerr << "calibration: " << iter_per_us << " iterations / microsecond\n";

    return iter_per_us;
}

void Workload::_run_for_n(uint64_t n)
{
    for (uint64_t i = 0; i < n; i++) {
        working_set_[__mm_crc32_u64(0xDEADBEEF, i) % dlines_] = 0x0A;
    }
}

/**
 * Model a memory workload by writing to n random bytes in some malloc'd
 * memory for the amount of time specified.
 */
void Workload::run_for_mem(chrono::microseconds d)
{
    _run_for_n(ITER_PER_US_ * (double)d.count());
}

/**
 * Spins the CPU for a given delay.
 */
void Workload::run_for_spin(chrono::microseconds d)
{
    auto t0 = chrono::steady_clock::now();
    while ((chrono::steady_clock::now() - t0) < d) {
    }
}

/**
 * Spins the CPU for a given delay.
 */
void Workload::run_for_sleep(chrono::microseconds d)
{
    this_thread::sleep_for(d);
}
