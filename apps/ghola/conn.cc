#include <vector>
#include <stdexcept>

#include "conn.hh"
#include "synthetic.hh"
#include "workload.hh"

static constexpr uint64_t WORKLOAD_SIZE = 1000;

Conn::Conn(connected_socket &&fd, std::vector<uint64_t> & iatimes, bool save_ia)
    : fd_{std::move(fd)}
    , rx_{fd_.input()}
    , tx_{fd_.output()}
    , iatimes_{iatimes}
    , save_ia_{save_ia}
    , last_ts_{}
    , wl_{WORKLOAD_SIZE}
    , rsp_{}
{
    fd_.set_nodelay(true);
}

future<stop_iteration> Conn::handle_request(void) {
    return rx_.read_exactly(sizeof(SynReq)).then([this] (temporary_buffer<char> buf) {
        if (not buf) {
            return make_ready_future<stop_iteration>(stop_iteration::yes);
        } else {
          const SynReq *req = reinterpret_cast<const SynReq *>(buf.get());
          if (save_ia_) {
              auto now = std::chrono::steady_clock::now();
              if (last_ts_ == std::chrono::steady_clock::time_point{}) {
                  last_ts_ = now;
              } else {
                  iatimes_.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_ts_).count());
                  last_ts_ = now;
              }
          }
          if (req->delays[0] > 0) {
              if (req->nr > REQ_MAX_DELAYS) {
                  throw std::runtime_error("run_request: too many delays in request");
              }
              // TODO: other servers allocate a new workload on each request...
              for (uint32_t i = 0; i < req->nr; i++) {
                  auto us = std::chrono::microseconds(req->delays[i] & REQ_DELAY_MASK);
                  if (req->delays[i] & REQ_DELAY_SLEEP) {
                      // TODO: not clear if should spin, sleep thread, or sleep future...
                      wl_.run_for_spin(us);
                  } else {
                      wl_.run_for_mem(us);
                  }
              }
          }
          if (req->nores == 0) {
              rsp_.tag = req->tag;
              return tx_.write(reinterpret_cast<const char *>(&rsp_), sizeof(SynRsp)).then([this] {
                  return tx_.flush().then([] {
                      return stop_iteration::no;
                  });
              });
          } else {
              return make_ready_future<stop_iteration>(stop_iteration::no);
          }
        }
    });
}

future<> Conn::run(void)
{
    return repeat([this] {
        return handle_request();
    }).then_wrapped([this] (auto &&f) {
        return when_all(tx_.close(), rx_.close(), fd_.shutdown_output(), fd_.shutdown_input()).then_wrapped([this] (auto &&f) {
            return make_ready_future();
        });
    });
}
