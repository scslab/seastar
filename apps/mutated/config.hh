#ifndef MUTATED_CONFIG_HH
#define MUTATED_CONFIG_HH

#include <cstdint>
#include <string>

namespace CONFIG
{
// Tunables
static constexpr uint64_t SEED = 1337;
static constexpr uint64_t REQ_OUT = 1024;
};

// Configuration values
struct Config {
    uint64_t seed;
    std::string server_addr;
    uint64_t nconns;
    uint64_t service_us;
    uint64_t req_per_s;
    uint64_t warm_s;
    uint64_t cool_s;
    uint64_t measure_s;
    std::string ia_file;
    bool send_only;
};

#endif /* MUTATED_CONFIG_HH */
