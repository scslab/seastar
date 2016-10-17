#ifndef MUTATED_CONN_HH
#define MUTATED_CONN_HH

#include "core/chunked_fifo.hh"
#include "core/reactor.hh"

#include "config.hh"
#include "results.hh"
#include "synthetic.hh"

class Conn
{
  private:
    connected_socket sock_;
    input_stream<char> rx_;
    output_stream<char> tx_;
    uint64_t rcvd_;
    uint64_t sent_;
    uint64_t errs_;
    SyntheticGenerator syngen_;
    Results &results_;
    chunked_fifo<SynOut, CONFIG::REQ_OUT> requests_;

  public:
    Conn(connected_socket &&sock, Results &results, uint64_t seed,
         uint64_t service_us, bool send_only);
    ~Conn(void){};

    Conn(Conn &&conn)
      : sock_{std::move(conn.sock_)}
      , rx_{std::move(conn.rx_)}
      , tx_{std::move(conn.tx_)}
      , rcvd_{0}
      , sent_{0}
      , errs_{0}
      , syngen_{std::move(conn.syngen_)}
      , results_{conn.results_}
      , requests_{}
    {
    }

    Conn(const Conn &) = delete;
    Conn &operator=(const Conn &) = delete;
    Conn &operator=(Conn &&) = delete;

    future<> recv_request(void);
    future<> send_request(bool measure);

    uint64_t rcvd(void) const noexcept { return rcvd_; }
    uint64_t sent(void) const noexcept { return sent_; }
    uint64_t errs(void) const noexcept { return errs_; }
};

#endif /* MUTATED_CONN_HH */
