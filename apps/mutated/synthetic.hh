#ifndef MUTATED_SYNTHETIC_HH
#define MUTATED_SYNTHETIC_HH

#include <cstdint>
#include <cstring>
#include <random>
#include <string>

#include "clock.hh"

static constexpr size_t REQ_MAX_DELAYS = 16;

/**
 * Synthetic protocol request type.
 */
struct SynReq {
    int nr; /* the number of delays */
    int nores;
    uint64_t tag;                    /* a unique indentifier for the request */
    uint64_t delays[REQ_MAX_DELAYS]; /* an array of delays */
} __attribute__((packed));

/**
 * Synthetic protocol response type.
 */
struct SynRsp {
    uint64_t tag;
} __attribute__((packed));

/**
 * Tracks an outstanding synthetic request.
 */
struct SynOut {
    uint64_t tag;
    bool measure;
    mut::time_point start_ts;
    uint64_t service_us;

    SynOut(uint64_t t, bool m, uint64_t s)
      : tag{t}
      , measure{m}
      , start_ts{mut::clock::now()}
      , service_us{s}
    {
    }
};

/**
 * Synthetic protocol packet generator.
 */
class SyntheticGenerator
{
  private:
    std::mt19937 rand_;
    std::exponential_distribution<double> service_dist_;
    bool send_only_;
    SynReq pkt_;
    uint64_t idx_;

    uint64_t gen_service_time(void) { return ceil(service_dist_(rand_)); }

  public:
    SyntheticGenerator(uint64_t seed, uint64_t service_us, bool send_only)
      : rand_{seed}
      , service_dist_{1.0 / service_us}
      , send_only_{send_only}
      , pkt_{}
      , idx_{0}
    {
        pkt_.nr = 1;
        memset(&pkt_, 0, sizeof(pkt_));
    }

    SyntheticGenerator(SyntheticGenerator &&s)
      : rand_{std::move(s.rand_)}
      , service_dist_{std::move(s.service_dist_)}
      , send_only_{false}
      , pkt_{std::move(s.pkt_)}
      , idx_{s.idx_}
    {
    }

    SyntheticGenerator(const SyntheticGenerator &) = delete;
    SyntheticGenerator &operator=(const SyntheticGenerator &) = delete;
    SyntheticGenerator &operator=(SyntheticGenerator &&) = delete;

    SynReq &operator()(void)
    {
        pkt_.tag = idx_++;
        pkt_.nores = send_only_;
        pkt_.delays[0] = gen_service_time();
        return pkt_;
    }
};

#endif /* MUTATED_SYNTHETIC_HH */
