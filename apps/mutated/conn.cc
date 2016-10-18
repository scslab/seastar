#include <chrono>

#include "core/reactor.hh"
#include <boost/range/irange.hpp>

#include "config.hh"
#include "conn.hh"
#include "results.hh"
#include "synthetic.hh"

Conn::Conn(connected_socket &&sock, Results &results, uint64_t seed,
           uint64_t service_us, bool send_only)
  : sock_{std::move(sock)}
  , rx_{sock_.input()}
  , tx_{sock_.output()}
  , rcvd_{0}
  , sent_{0}
  , errs_{0}
  , syngen_{seed, service_us, send_only}
  , results_{results}
  , requests_{}
{
    sock_.set_nodelay(true);
}

future<> Conn::recv_request(void)
{
    return rx_.read_exactly(sizeof(SynRsp))
      .then([this](temporary_buffer<char> buf) {
          const SynRsp *rsp = reinterpret_cast<const SynRsp *>(buf.get());
          SynOut req = requests_.front();
          if (rsp->tag != req.tag) {
              std::cerr << "wrong tag! " << rsp->tag << " vs " << req.tag
                        << "\n";
              exit(EXIT_FAILURE);
          }
          if (req.measure) {
            auto t1 = mut::clock::now();
            uint64_t service_us =
              std::chrono::duration_cast<mut::us>(t1 - req.start_ts).count();
            uint64_t wait_us = service_us - req.service_us;
            results_.add_sample(service_us, wait_us, sizeof(SynRsp));
            // TODO: unify results settting functions
          }
          rcvd_++;
          requests_.pop_front();
          return make_ready_future<>();
      });
}

future<> Conn::send_request(bool measure)
{
    SynReq &r = syngen_();
    uint64_t tag = r.tag;
    uint64_t del = r.delays[0];
    requests_.emplace_back(tag, measure, del);
    return tx_.write((char *)&r, sizeof(SynReq))
      .then_wrapped([this](auto &&f) {
          // TODO: unify results settting functions
          results_.add_tx(sizeof(SynReq));
          sent_++;
          try {
              f.get();
              return tx_.flush();
          } catch (...) {
              errs_++;
              std::cerr << "send error!\n";
              exit(EXIT_FAILURE);
          }
      });
}
