#ifndef MUTATED_WORKER_HH
#define MUTATED_WORKER_HH

#include "core/future.hh"
#include "core/reactor.hh"

#include "config.hh"
#include "conn.hh"
#include "schedule.hh"

class Worker
{
  private:
    Config cfg_;
    std::vector<lw_shared_ptr<Conn>> conns_;
    Schedule schedule_;
    uint64_t rcvd_reqs_;
    promise<> finished_;
    Results results_;

    lw_shared_ptr<Conn> get_connection(void);
    future<> setup_connections(void);
    future<> run_recv(void);
    future<stop_iteration> send_request(void);
    future<> run_send(void);
    future<> finish(void);

  public:
    explicit Worker(Config &cfg);
    ~Worker(void){};

    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;
    Worker(Worker &&) = delete;
    Worker &operator=(Worker &&) = delete;

    future<> stop(void);
    future<> run(void);
};

#endif /* MUTATED_WORKER_HH */
