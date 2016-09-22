#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/reactor.hh"

using namespace net;

class tcp_server {
public:
  future<> start(uint16_t port)  {
    return make_ready_future<>();
  }

  future<> stop() {
    return make_ready_future<>();
  }
}

namespace bpo = boost::program_options;

int main(int ac, char** av) {
  // server options
  app_template app;
  app.add_options()
    ("port", po::value<uint16_t>()->default_value(8080), "TCP server port")
    ;

  // run seastar
  return app.run_deprecated(ac, av, [&] {
    auto&& config = app.configuration();
    uint16_t port = config["port"].as<uint16_t>();
    auto server = new distributed<tcp_server>;

    server->start().then([server = std::move(server), port] () mutable {
      engine().at_exit([server] {
        return server->stop();
      });
      server->invoke_on_all(&tcp_server::start, port);
    }).then([port] {
      std::cout << "TCP server listening on port " << port << " ...\n";
    });
  };
}
