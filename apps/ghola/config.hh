#ifndef GHOLA_CONFIG_HH
#define GHOLA_CONFIG_HH

#include <cstdint>
#include <string>

// Configuration values
struct Config {
    uint16_t port;
    std::string ia_file;
};

#endif /* GHOLA_CONFIG_HH */
