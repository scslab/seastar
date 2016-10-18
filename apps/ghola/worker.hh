#ifndef GHOLA_WORKER_HH
#define GHOLA_WORKER_HH

#include "core/reactor.hh"
#include "config.hh"

class Worker
{
private:
    Config cfg_; 
    server_socket sock_;

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
