#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

#include "accum.hh"

// TODO: Histogram rather than all samples?

using namespace std;

void Accum::clear(void)
{
    samples_.clear();
    sorted_ = false;
}

void Accum::add_sample(uint64_t val)
{
    samples_.push_back(val);
    sorted_ = false;
}

void Accum::print_samples(void) const noexcept
{
    for (auto i : samples_) {
        cout << i << endl;
    }
}

double Accum::mean(void) const noexcept
{
    double avg = 0;
    double sz = size();

    for (auto i : samples_) {
        avg += double(i) / sz;
    }

    return avg;
}

double Accum::stddev(void) const noexcept
{
    double avg = mean();
    double sum = 0;

    for (auto i : samples_) {
        double diff = double(i) - avg;
        sum += diff * diff;
    }

    return sqrt(sum / size());
}

uint64_t Accum::percentile(double percent) const noexcept
{
    if (not sorted_) {
        sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
    return samples_[ceil(double(size()) * percent) - 1];
}

uint64_t Accum::min(void) const noexcept
{
    if (not sorted_) {
        sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
    return samples_[0];
}

uint64_t Accum::max(void) const noexcept
{
    if (not sorted_) {
        sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
    return samples_[samples_.size() - 1];
}

vector<uint64_t>::size_type Accum::size(void) const noexcept
{
    return samples_.size();
}
