#ifndef GHOLA_CONN_HH
#define GHOLA_CONN_HH

#include "core/reactor.hh"
#include "synthetic.hh"
#include "workload.hh"

class Conn
{
private:
    connected_socket fd_;
    input_stream<char> rx_;
    output_stream<char> tx_;
    Workload wl_;
    SynRsp rsp_;

    future<stop_iteration> handle_request(void);

public:
    explicit Conn(connected_socket &&fd);
    ~Conn(void) = default;

    Conn(const Conn &) = delete;
    Conn &operator=(const Conn &) = delete;
    Conn(Conn &&) = delete;
    Conn &operator=(Conn &&) = delete;

    future<> run(void);
};

#endif /* GHOLA_CONN_HH */
