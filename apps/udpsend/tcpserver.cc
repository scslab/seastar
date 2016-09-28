#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/reactor.hh"

constexpr uint64_t PKT_SIZE = 4;

using namespace net;
using namespace seastar;

class tcp_conn {
private:
  connected_socket _fd;
  input_stream<char> _rx;
  output_stream<char> _tx;;

  future<stop_iteration> pingpong(void) {
    return _rx.read_exactly(PKT_SIZE).then([this] (temporary_buffer<char> buf) {
      if (buf) {
        return _tx.write(std::move(buf)).then([this] {
          return _tx.flush().then([] {
            return stop_iteration::no;
          });
        });
      } else {
        return make_ready_future<stop_iteration>(stop_iteration::yes);
      }
    });
  }

public:
  tcp_conn(connected_socket &&fd)
    : _fd{std::move(fd)}
    , _rx{_fd.input()}
    , _tx{_fd.output()}
  {}

  future<> run(void) {
    return repeat([this] {
      return pingpong();
    }).then([this] {
      return _tx.close();
    });
  }
};

class tcp_server {
private:
  server_socket _sock;

  future<> run_client(connected_socket &&fd) {
    return do_with(new tcp_conn(std::move(fd)), [] (auto & conn) {
      return conn->run();
    });
  }

public:
  future<> start(uint16_t port)  {
    ipv4_addr addr{port};
    listen_options lo;
    lo.proto = transport::TCP;
    lo.reuse_address = true;
    _sock = engine().listen(make_ipv4_address(addr), lo);

    return keep_doing([this] {
      return _sock.accept().then([this] (connected_socket fd, socket_address a) {
        // XXX: Don't return client future if want handle more than one at time
        return run_client(std::move(fd));
      });
    });
  }

  future<> stop() {
    // TODO: Handle shutdown
    return make_ready_future<>();
  }
};

namespace po = boost::program_options;

int main(int ac, char** av) {
  app_template app;
  app.add_options()
    ("port", po::value<uint16_t>()->default_value(8080), "TCP server port")
    ;

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
  });
}
