#include "bargain_v0/bargain.hpp"

#include "testing_v1/test.hpp"

#include "dumpster_v1/ranqd1.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace testing_v1;
using namespace bargain_v0;

auto contention_test = test([]() {
  const size_t n_threads = std::thread::hardware_concurrency();
  const size_t n_ops = 1000000;

  std::mutex mutex;
  std::condition_variable condition;

  size_t n_threads_started = 0;
  size_t n_threads_stopped = 0;

  const size_t n_atoms = 131071;

  std::unique_ptr<STM::atom_t<int>[]> atoms(new STM::atom_t<int>[n_atoms]);
  for (size_t i = 0; i < n_atoms; ++i)
    STM::run(atoms[i] = 0);

  for (size_t t = 0; t < n_threads; ++t) {
    std::thread([&, t]() {
      {
        std::unique_lock<std::mutex> guard(mutex);
        n_threads_started += 1;
        condition.notify_all();
        while (n_threads_started != n_threads)
          condition.wait(guard);
      }

      auto s = static_cast<uint32_t>(t);

      for (size_t o = 0; o < n_ops; ++o) {
        auto i = (s = dumpster_v1::ranqd1(s)) % n_atoms;
        auto j = i;
        while (i == j)
          j = (s = dumpster_v1::ranqd1(s)) % n_atoms;

        STM::run(*atoms[i] >> [&](auto &x) {
          return *atoms[j] >> [&](auto &y) {
            return (atoms[j] = x - 1) >>
                   [&](auto &) { return atoms[i] = y + 1; };
          };
        });
      }

      {
        std::unique_lock<std::mutex> guard(mutex);
        n_threads_stopped += 1;
        condition.notify_all();
      }
    }).detach();
  }

  {
    std::unique_lock<std::mutex> guard(mutex);
    while (n_threads_stopped != n_threads)
      condition.wait(guard);
  }

  int sum = 0;
  for (size_t i = 0; i < n_atoms; ++i)
    sum += STM::run(*atoms[i]);

  verify(sum == 0);
});
