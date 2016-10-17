#ifndef MUTATED_CLOCK_HH
#define MUTATED_CLOCK_HH

#include <chrono>

namespace mut
{
using clock = std::chrono::steady_clock;
using time_point = clock::time_point;
using ns = std::chrono::nanoseconds;
using us = std::chrono::microseconds;
using ms = std::chrono::milliseconds;
using sec = std::chrono::seconds;

static constexpr double NSEC = 1000000000;
};

#endif /* MUTATED_CLOCK_HH */
