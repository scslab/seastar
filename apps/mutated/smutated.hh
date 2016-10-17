#ifndef MUTATED_PLAY_HH
#define MUTATED_PLAY_HH

#include "core/app-template.hh"
#include "core/distributed.hh"
#include "worker.hh"

class SMutated
{
  private:
    app_template app_;
    std::unique_ptr<distributed<Worker>> workers_;
    Config cfg_;

    void parse_config(void);

  public:
    SMutated(void);
    ~SMutated(void){};

    SMutated(const SMutated &) = delete;
    SMutated &operator=(const SMutated &) = delete;
    SMutated(SMutated &&) = delete;
    SMutated &operator=(SMutated &&) = delete;

    int run(int argc, char **argv);
};

#endif /* MUTATED_PLAY_HH */
