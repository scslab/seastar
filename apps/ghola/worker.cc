#include <fstream>

#include "core/reactor.hh"

#include "config.hh"
#include "conn.hh"
#include "worker.hh"

using namespace net;
using namespace seastar;

Worker::Worker(Config & cfg)
    : cfg_{cfg}
    , sock_{}
    , iatimes_{}
{
    iatimes_.reserve(IATIMES_RESERVE);
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
            bool save_ia = not cfg_.ia_file.empty();
            return do_with(std::make_unique<Conn>(std::move(fd), iatimes_, save_ia), [] (auto & conn) {
                return conn->run();
            });
        });
    });
}

future<> Worker::stop(void)
{
    if (iatimes_.size() > 0) {
        std::string ia_file = cfg_.ia_file + "_" + std::to_string(engine().cpu_id());
        std::ofstream f(ia_file);
        for (auto t : iatimes_) {
            f << t << "\n";
        }
        f.close();
    }
    return make_ready_future<>();
}
