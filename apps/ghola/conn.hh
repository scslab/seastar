#ifndef GHOLA_CONN_HH
#define GHOLA_CONN_HH

#include <vector>

#include "core/reactor.hh"
#include "synthetic.hh"
#include "workload.hh"

class Conn
{
private:
    connected_socket fd_;
    input_stream<char> rx_;
    output_stream<char> tx_;
    std::vector<uint64_t> & iatimes_;
    bool save_ia_;
    std::chrono::steady_clock::time_point last_ts_;
    Workload wl_;
    SynRsp rsp_;

    future<stop_iteration> handle_request(void);

public:
    Conn(connected_socket &&fd, std::vector<uint64_t> & iatimes, bool save_ia);
    ~Conn(void) = default;

    Conn(const Conn &) = delete;
    Conn &operator=(const Conn &) = delete;
    Conn(Conn &&) = delete;
    Conn &operator=(Conn &&) = delete;

    future<> run(void);
};

#endif /* GHOLA_CONN_HH */
