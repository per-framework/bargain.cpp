#include "bargain_v0/bargain.hpp"

#include "testing_v1/test.hpp"

using namespace testing_v1;
using namespace bargain_v0;

auto smoke_test = test([]() {
  STM::atom_t<int> xA(1);
  STM::atom_t<float> yA(2.0f);
  STM::atom_t<int> zA(3);

  {
    verify(1 == STM::run(*xA));
    verify(2 == STM::run(*yA));
    verify(3 == STM::run(*zA));
  }

  {
    auto op = *xA >> [&](auto &x) {
      return *zA >> [&](auto &z) {
        return (xA = z) >> [&](auto &) { return (zA = x); };
      };
    };

    static_assert(std::is_same_v<STM::unit_t, STM::result_of_t<decltype(op)>>);

    STM::run(op);

    verify(3 == STM::run(*xA));
    verify(2 == STM::run(*yA));
    verify(1 == STM::run(*zA));
  }

  {
    auto op = *xA >> [](auto &x) { return STM::pure(x + 1); };

    static_assert(std::is_same_v<int, STM::result_of_t<decltype(op)>>);

    auto r = STM::run(op);

    verify(r == 4);
    verify(3 == STM::run(*xA));
    verify(2 == STM::run(*yA));
    verify(1 == STM::run(*zA));
  }

  {
    auto op = *xA >> [&](auto &x) {
      return *yA >> [&](auto &y) { return STM::pure(x + y); };
    };

    static_assert(std::is_same_v<float, STM::result_of_t<decltype(op)>>);

    auto r = STM::run(op);

    verify(r == 5.f);
    verify(3 == STM::run(*xA));
    verify(2 == STM::run(*yA));
    verify(1 == STM::run(*zA));
  }

  // verify(false);
});
