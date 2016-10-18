#include "core/app-template.hh"
#include "core/distributed.hh"
#include "core/reactor.hh"

#include "server.hh"

namespace po = boost::program_options;

Server::Server(void)
    : app_{}
    , workers_{}
    , cfg_{}
{
  app_.add_options()
    ("port", po::value<uint16_t>()->default_value(8080), "TCP server port")
    ("ia-file", po::value<std::string>(), "save iatimes to file")
    ;
}

void Server::parse_config(void)
{
    auto&& config = app_.configuration();
    cfg_.port = config["port"].as<uint16_t>();
    if (config.count("ia-file")) {
        cfg_.ia_file = config["ia-file"].as<std::string>();
    }
    workers_ = std::make_unique<distributed<Worker>>();
}

int Server::run(int ac, char *av[])
{
    return app_.run_deprecated(ac, av, [&] {
        parse_config();
        return workers_->start(cfg_).then([&]() mutable {
            engine().at_exit([&] { return workers_->stop(); });
            return workers_->invoke_on_all(&Worker::start);
        });
    });
}
