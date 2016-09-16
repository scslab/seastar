#ifndef _STDDEV_HH
#define _STDDEV_HH

/*
 * Basic statistics module: standard deviation routines.
 */

class accum
{
private:
  int _min;
  int _max;
  int64_t _sum;
  uint64_t _sum_sq;
  uint64_t _count;

public:
  accum(void)
    : _min{INT_MAX}, _max{INT_MIN}, _sum{0}, _sum_sq{0}, _count{0}
  {}

  void add(int value)
  {
    _count++;
    _sum += value;
    _sum_sq += value * value;
    if (value < _min) {
      _min = value;
    } else if (value > _max) {
      _max = value;
    }
  }

  void clear(void) {
    _min = INT_MAX;
    _max = INT_MIN;
    _sum = _sum_sq = _count = 0;
  }

  int min(void) const noexcept { return _min; }

  int max(void) const noexcept { return _max; }

  uint64_t count(void) const noexcept { return _count; }

  double average(void) const noexcept
  {
    return double(_sum) / double(_count);
  }

  double stddev(void) const noexcept
  {
    if (_count > 2) {
      double sq = _sum_sq, s = _sum, c = _count;
      double variance = (sq - (s * (s / c))) / c;
      return sqrt(fabs(variance));
    } else {
      return 0.0;
    }
  }
};

#endif // _STDDEV_HH
