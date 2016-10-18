#ifndef GHOLA_WORKER_HH
#define GHOLA_WORKER_HH

#include <vector>

#include "core/reactor.hh"
#include "config.hh"

class Worker
{
private:
    // XXX: this only works when we have one connection per worker...
    static constexpr size_t IATIMES_RESERVE = 50000000;

    Config cfg_; 
    server_socket sock_;
    std::vector<uint64_t> iatimes_;

public:
    explicit Worker(Config & cfg);
    ~Worker(void) = default;

    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;
    Worker(Worker &&) = delete;
    Worker &operator=(Worker &&) = delete;

    future<> start(void);
    future<> stop(void);
};

#endif /* GHOLA_WORKER_HH */
