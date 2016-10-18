#ifndef MUTATED_SCHEDULE_HH
#define MUTATED_SCHEDULE_HH

#include <chrono>
#include <random>
#include <vector>

#include "clock.hh"
#include "config.hh"

class Schedule
{
  private:
    uint64_t reqs_pers_;
    uint64_t warm_secs_;
    uint64_t cool_secs_;

    uint64_t warm_reqs_{0};
    uint64_t meas_reqs_;
    uint64_t cool_reqs_{0};

    std::vector<mut::ns> deadlines_{};
    std::mt19937 randgen_;

    mut::time_point start_{};
    uint64_t sent_{0};
    uint64_t rcvd_{0};

  public:
    explicit Schedule(Config &cfg)
      : reqs_pers_{cfg.req_per_s}
      , warm_secs_{cfg.warm_s}
      , cool_secs_{cfg.cool_s}
      , meas_reqs_{cfg.req_per_s * cfg.measure_s}
      , randgen_{cfg.seed}
    {
        // Exponential distribution suggested by experimental evidence, c.f.
        // Figure
        // 11 in "Power Management of Online Data-Intensive Services"
        std::exponential_distribution<double> d(1.0 /
                                                (mut::NSEC / reqs_pers_));
        double accum = 0;
        uint64_t pos = 0;
        mut::ns coolstart;

        // generate warm-up samples
        while (mut::ns(uint64_t(ceil(accum))) < mut::sec(warm_secs_)) {
            accum += d(randgen_);
            deadlines_.push_back(mut::ns(uint64_t(ceil(accum))));
            pos++;
        }
        warm_reqs_ = pos;

        // generate measurement samples
        while (pos - warm_reqs_ < meas_reqs_) {
            accum += d(randgen_);
            deadlines_.push_back(mut::ns(uint64_t(ceil(accum))));
            pos++;
        }

        // generate cool-down samples
        coolstart = mut::ns(uint64_t(ceil(accum)));
        while (mut::ns(uint64_t(ceil(accum))) - coolstart <
               mut::sec(cool_secs_)) {
            accum += d(randgen_);
            deadlines_.push_back(mut::ns(uint64_t(ceil(accum))));
            pos++;
        }
        cool_reqs_ = pos - warm_reqs_ - meas_reqs_;
    }

    Schedule(Schedule &&) = delete;
    Schedule(const Schedule &) = delete;
    Schedule &operator=(Schedule &&) = delete;
    Schedule &operator=(const Schedule &) = delete;

    bool start_measure(void) const noexcept { return sent_ == warm_reqs_; }
    bool measure_request(void) const noexcept
    {
        return sent_ >= warm_reqs_ and sent_ < (warm_reqs_ + meas_reqs_);
    }
    bool end_measure(void) const noexcept { return rcvd_ == warm_reqs_ + meas_reqs_; }
    bool finished(void) const noexcept { return sent_ >= deadlines_.size(); }
    bool end_experiment(void) const noexcept { return rcvd_ == deadlines_.size(); }

    void start(void) noexcept { start_ = mut::clock::now(); }
    mut::ns next(void) noexcept
    {
        auto now_relative = mut::clock::now() - start_;
        return deadlines_[sent_++] - now_relative;
    }
    void rcvd_response(void) noexcept { rcvd_++; }
    std::vector<mut::ns> &deadlines(void) { return deadlines_; }

    uint64_t req_pers(void) const noexcept { return reqs_pers_; }
    uint64_t samples(void) const noexcept { return meas_reqs_; }
};

#endif /* MUTATED_SCHEDULE_HH */
