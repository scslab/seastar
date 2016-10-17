#ifndef MUTATED_ACCUM_HH
#define MUTATED_ACCUM_HH

#include <cstdint>
#include <vector>

/**
 * A sample accumulator container.
 */
class Accum
{
  private:
    mutable std::vector<uint64_t> samples_;
    mutable bool sorted_;

  public:
    using size_type = std::vector<uint64_t>::size_type;

    Accum(void) noexcept : samples_{}, sorted_{false} {}

    explicit Accum(std::size_t reserve) noexcept : samples_{reserve},
                                                   sorted_{false}
    {
        // we want to zero out the memory to ensure it's paged in, but we still
        // want to use the push_back operator for its bounds-checking a growth.
        samples_.resize(0);
    }

    ~Accum(void) noexcept {}

    void clear(void);
    void add_sample(uint64_t val);
    void print_samples(void) const noexcept;

    double mean(void) const noexcept;
    double stddev(void) const noexcept;
    uint64_t percentile(double percent) const noexcept;
    uint64_t min(void) const noexcept;
    uint64_t max(void) const noexcept;
    size_type size(void) const noexcept;
};

#endif /* MUTATED_ACCUM_HH */
