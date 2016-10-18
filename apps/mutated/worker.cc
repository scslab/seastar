#include "core/future.hh"
#include "core/reactor.hh"
#include "core/sleep.hh"

#include "clock.hh"
#include "config.hh"
#include "conn.hh"
#include "schedule.hh"
#include "worker.hh"

#include <fstream>

using namespace net;
using namespace seastar;

Worker::Worker(Config &cfg)
  : cfg_{cfg}
  , conns_{}
  , schedule_{cfg}
  , finished_{}
  , results_{schedule_.samples()}
{
    conns_.reserve(cfg_.nconns);
}

future<> Worker::stop()
{
  return make_ready_future();
}

lw_shared_ptr<Conn> Worker::get_connection(void)
{
    static int idx = 0;
    return conns_[idx++ % conns_.size()];
}

future<> Worker::setup_connections(void)
{
    auto range = boost::irange(0, (int)cfg_.nconns);
    auto addr = make_ipv4_address(cfg_.server_addr);
    socket_address local =
      socket_address(::sockaddr_in{AF_INET, INADDR_ANY, {0}});

    return parallel_for_each(range, [this, addr, local](int) {
        return engine()
          .net()
          .connect(addr, local, transport::TCP)
          .then([this](connected_socket fd) {
              conns_.emplace_back(
                make_lw_shared<Conn>(std::move(fd), results_, cfg_.seed,
                                     cfg_.service_us, cfg_.send_only));
              return make_ready_future();
          });
    });
}

// TODO: how to cancel outstanding reads?
// TODO: better to only wait on read for connections with outstanding requests?
future<> Worker::run_recv(void)
{
    if (cfg_.send_only) {
        return make_ready_future();
    }

    return parallel_for_each(conns_, [this](lw_shared_ptr<Conn> c) {
        return keep_doing([this, c] {
            return c->recv_request().then([this] {
                schedule_.rcvd_response();
                if (schedule_.end_measure()) {
                    // TODO: unify results settting functions
                    results_.end_measurements();
                } else if (schedule_.end_experiment()) {
                    finished_.set_value();
                }
                return make_ready_future();
            });
        });
    });
}

future<stop_iteration> Worker::send_request(void)
{
    auto c = get_connection();
    bool m = schedule_.measure_request();
    if (schedule_.start_measure()) {
        // TODO: unify results settting functions
        results_.start_measurements();
    }
    return c->send_request(m).then([] { return stop_iteration::no; });
}

// round-robbin send all requests
// TODO: Detect and behave responsibly if we can't keep up with the schedule...
// TODO: Should the schedule include some constant subtraction for dealing with
// the time spent sending...
future<> Worker::run_send(void)
{
    schedule_.start();
    return repeat([this] {
        if (schedule_.finished()) {
            if (cfg_.send_only) {
                results_.end_measurements();
                finished_.set_value();
                // XXX: Hack, wait 1 second to ensure all sends complete...
                // write + flush returns before actually written to wire, so
                // not clear the correct way to wait...
                return sleep(mut::sec(1)).then([] {
                    return stop_iteration::yes;
                });
            } else {
                return make_ready_future<stop_iteration>(stop_iteration::yes);
            }
        } else {
            auto d = schedule_.next();
            if (d < mut::ns(0)) {
                return send_request();
            } else {
                return sleep(d).then([this] { return send_request(); });
            }
        }
    });
}

future<> Worker::finish(void)
{
    if (not cfg_.send_only) {
        results_.print(schedule_.req_pers());
    }

    if (cfg_.ia_file != "") {
        std::string ia_file =
          cfg_.ia_file + "_" + std::to_string(engine().cpu_id());
        mut::ns last{0};
        std::ofstream f(ia_file);
        for (auto t : schedule_.deadlines()) {
            if (last == mut::ns(0)) {
                last = t;
            } else {
                f << (t - last).count() << "\n";
                last = t;
            }
        }
        f.close();
    }

    return make_ready_future();
}

future<> Worker::run(void)
{
    return setup_connections()
      .then([this] {
          run_recv();
          return run_send();
      })
      .then([this] {
          // finish
          return finished_.get_future().then([this] { return finish(); });
      });
}
