#include "core/reactor.hh"

#include "config.hh"
#include "conn.hh"
#include "worker.hh"

using namespace net;
using namespace seastar;

Worker::Worker(Config & cfg)
    : cfg_{cfg}
    , sock_{}
{
}

future<> Worker::start(void)
{
    ipv4_addr addr{cfg_.port};
    listen_options lo;
    lo.proto = transport::TCP;
    lo.reuse_address = true;
    sock_ = engine().listen(make_ipv4_address(addr), lo);

    return keep_doing([this] {
        return sock_.accept().then([this] (connected_socket fd, socket_address a) {
            return do_with(std::make_unique<Conn>(std::move(fd)), [] (auto & conn) {
                return conn->run();
            });
        });
    });
}

future<> Worker::stop(void)
{
    return make_ready_future<>();
}
