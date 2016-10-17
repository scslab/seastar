#include "smutated.hh"
#include "config.hh"
#include "core/reactor.hh"

using namespace net;
using namespace seastar;

namespace po = boost::program_options;

SMutated::SMutated(void)
  : app_{}
  , workers_{}
  , cfg_{}
{
    app_.add_options()("server", po::value<std::string>(), "Server address")(
      "conn", po::value<uint64_t>()->default_value(1),
      "# connections per cpu")("reqs", po::value<uint64_t>(), "requests/s")(
      "service", po::value<uint64_t>(), "expected service time at server")(
      "warm", po::value<uint64_t>()->default_value(1), "warmup time")(
      "cool", po::value<uint64_t>()->default_value(1), "cooldown time")(
      "measure", po::value<uint64_t>()->default_value(5), "measure time")(
      "ia-file", po::value<std::string>(),
      "save iatimes to file")("send-only", "send requests only, no responses");
}

void SMutated::parse_config(void)
{
    auto &&config = app_.configuration();

    if (config.count("service") == 0) {
        std::cerr << "Missing '--service' argument\n";
        engine().exit(EXIT_FAILURE);
    } else if (config.count("server") == 0) {
        std::cerr << "Missing '--server' argument\n";
        engine().exit(EXIT_FAILURE);
    }

    cfg_.seed = CONFIG::SEED;
    cfg_.server_addr = config["server"].as<std::string>();
    cfg_.nconns = config["conn"].as<uint64_t>();
    cfg_.req_per_s = config["reqs"].as<uint64_t>();
    cfg_.service_us = config["service"].as<uint64_t>();
    cfg_.warm_s = config["warm"].as<uint64_t>();
    cfg_.cool_s = config["cool"].as<uint64_t>();
    cfg_.measure_s = config["measure"].as<uint64_t>();
    cfg_.send_only = config.count("send-only");
    if (config.count("ia-file")) {
        cfg_.ia_file = config["ia-file"].as<std::string>();
    }
    workers_ = std::make_unique<distributed<Worker>>();
}

int SMutated::run(int argc, char **argv)
{
    return app_.run(argc, argv, [&] {
        parse_config();
        return workers_->start(cfg_).then([&]() mutable {
            engine().at_exit([&] { return workers_->stop(); });
            return workers_->invoke_on_all(&Worker::run);
        });
    });
}
