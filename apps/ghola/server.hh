#ifndef GHOLA_SERVER_HH
#define GHOLA_SERVER_HH

#include "core/app-template.hh"
#include "core/distributed.hh"
#include "worker.hh"

class Server
{
private:
    app_template app_;
    std::unique_ptr<distributed<Worker>> workers_;
    Config cfg_;

    void parse_config(void);

public:
    Server(void);
    ~Server(void) = default;

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

    int run(int ac, char *av[]);
};

#endif /* GHOLA_SERVER_HH */
